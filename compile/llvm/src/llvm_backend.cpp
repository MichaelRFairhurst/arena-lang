#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/TargetRegistry.h"
#include "arena_backend.hpp"
#include "ast/visitor.hpp"
#include "arena_llvm_types.hpp"

namespace {
    class LLVMBackendAstVisitor : public arena::ast::Visitor {
    public:
        LLVMBackendAstVisitor(llvm::LLVMContext &context,
                              llvm::Module &module,
                              llvm::IRBuilder<> &builder,
                              const arena::sema::ResolvedDeclaration *current_decl,
                              const arena::sema::FunctionTable *ftable,
                              const arena::sema::TypeTable *ttable)
            : context(&context), module(&module), builder(&builder), current_decl(current_decl),
              ftable(ftable), ttable(ttable) {}

    private:
        llvm::LLVMContext *context;
        llvm::Module *module;
        llvm::IRBuilder<> *builder;
        const arena::sema::ResolvedExpression *current_expr = nullptr;
        const arena::sema::ResolvedDeclaration *current_decl;
        llvm::Value *current_value = nullptr;
        const arena::sema::FunctionTable *ftable;
        const arena::sema::TypeTable *ttable;
        // TODO: use VariableId instead of string for the variable_map key
        std::unordered_map<std::string, llvm::Value *> variable_map;

        llvm::Type *getLLVMType(const arena::sema::TypeId type_id) {
            arena::llvm::TypeResolver type_resolver{builder, ttable, &current_decl->lifetimes};
            return type_resolver.getLLVMType(type_id);
        }

        llvm::FunctionType *getLLVMFunctionType(const arena::sema::ResolvedFunction &func) {
            llvm::Type *return_type = getLLVMType(func.get_return_type().value_or(
                ttable->get_type_id(arena::sema::VoidTypeSymbol{})));

            std::vector<llvm::Type *> param_types;
            for (const auto &param_type_id : *func.get_param_types()) {
                param_types.push_back(getLLVMType(param_type_id));
            }

            return llvm::FunctionType::get(return_type, param_types, false);
        }

        llvm::Function *declare_function(const arena::ast::FunctionDeclaration *node) {
            // TODO: Don't require looking up the function via the function table; we should have
            // this information available directly from the resolved declaration.
            auto func = ftable->resolve(node->get_name());
            if (!func) {
                throw std::runtime_error("Function not found in function table: " +
                                         std::string(node->get_name()));
            }

            auto func_type = getLLVMFunctionType(*func);

            llvm::Function *function = llvm::Function::Create(func_type,
                                                              llvm::Function::ExternalLinkage,
                                                              node->get_name(),
                                                              *module);
            return function;
        }

        void visit(const arena::ast::FunctionDeclaration *node) override { declare_function(node); }

        void visit(const arena::ast::FunctionDefinition *node) override {
            llvm::Function *function = declare_function(node);

            auto args = function->arg_begin();

            // TODO: get actual variable id from the resolved function.
            arena::sema::VariableId variable_id{0};
            for (auto &param : node->get_params()->get_params()) {
                auto name = param->get_name();
                args->setName(name);
                // TODO: Use VariableId instead of string for the variable_map key
                variable_map[std::string{name}] = &*args;
                ++variable_id.v_id;
                ++args;
            }

            // Create a basic block and set the insertion point
            llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", function);
            builder->SetInsertPoint(entry);

            visitStatement(current_decl->resolved_stmt);
        }

        void visitExpression(const arena::sema::ResolvedExpression *node) {
            current_expr = node;
            node->original->accept(this);
        }

        class ResolvedStatementVisitor {
        public:
            ResolvedStatementVisitor(llvm::LLVMContext *context,
                                     llvm::IRBuilder<> *builder,
                                     LLVMBackendAstVisitor *visitor)
                : context(context), builder(builder), visitor(visitor) {}

            void operator()(const arena::sema::ResolvedIfStatement &resolved_stmt) {
                visitor->visitExpression(resolved_stmt.condition);
                auto true_block = llvm::BasicBlock::Create(*context, "true_block");
                auto false_block = llvm::BasicBlock::Create(*context, "false_block");
                auto merge_block = llvm::BasicBlock::Create(*context, "merge_block");

                builder->CreateCondBr(visitor->current_value, true_block, false_block);

                builder->SetInsertPoint(true_block);
                visitor->visitStatement(resolved_stmt.then_branch);
                builder->CreateBr(merge_block);

                builder->SetInsertPoint(false_block);
                if (resolved_stmt.else_branch) {
                    visitor->visitStatement(resolved_stmt.else_branch);
                }
                builder->CreateBr(merge_block);

                builder->SetInsertPoint(merge_block);
            }

            void operator()(const arena::sema::ResolvedLetStatement &resolved_stmt) {
                visitor->visitExpression(resolved_stmt.initializer);
                auto value = visitor->current_value;
                // TODO: Use VariableId instead of string for the variable_map key
                visitor->variable_map[std::string{resolved_stmt.original->get_name()}] = value;
            }

            void operator()(const arena::sema::ResolvedReturnStatement &resolved_stmt) {
                visitor->visitExpression(resolved_stmt.expr);
                builder->CreateRet(visitor->current_value);
            }

            void operator()(const arena::sema::ResolvedBlockStatement &resolved_stmt) {
                for (size_t i = 0; i < resolved_stmt.num_statements; ++i) {
                    visitor->visitStatement(&resolved_stmt.statements[i]);
                }
            }

            void operator()(const arena::sema::ResolvedArenaStatement &resolved_stmt) {
                throw std::runtime_error("ResolvedArenaStatement not implemented");
            }

            void operator()(const arena::sema::ResolvedExprStatement &resolved_stmt) {
                visitor->visitExpression(resolved_stmt.expr);
            }

        private:
            llvm::LLVMContext *context;
            llvm::IRBuilder<> *builder;
            LLVMBackendAstVisitor *visitor;
        };

        void visitStatement(arena::sema::ResolvedStatement *node) {
            std::visit(ResolvedStatementVisitor{context, builder, this}, node->info);
        }

        void visit(const arena::ast::StringLiteral *node) override {
            throw std::runtime_error("StringLiteral not implemented");
        }

        void visit(const arena::ast::IntegerLiteral *node) override {
            throw std::runtime_error("IntegerLiteral not implemented");
        }

        void visit(const arena::ast::Literal *node) override {
            auto value = node->begin()->literalValue;
            if (auto int_value = std::get_if<int64_t>(&value)) {
                current_value = llvm::ConstantInt::get(*context, llvm::APInt(64, *int_value));
            } else if (auto string_value = std::get_if<std::string_view>(&value)) {
                current_value = builder->CreateGlobalStringPtr(*string_value);
            } else if (node->begin()->type == arena::ast::TokenType::TRUE) {
                // TODO: use a size of 1 bit
                current_value = llvm::ConstantInt::get(*context, llvm::APInt(8, 1));
            } else if (node->begin()->type == arena::ast::TokenType::FALSE) {
                // TODO: use a size of 1 bit
                current_value = llvm::ConstantInt::get(*context, llvm::APInt(8, 0));
            } else {
                throw std::runtime_error("Expected integer or string literal");
            }
        }

        void visit(const arena::ast::LiteralExpression *node) override {
            node->get_literal()->accept(this);
        }

        void visit(const arena::ast::IdExpression *node) override {
            // TODO: Use VariableId instead of string for the variable_map key
            auto it = variable_map.find(std::string{node->get_id()});
            if (it != variable_map.end()) {
                current_value = it->second;
            } else {
                throw std::runtime_error("Variable not found in variable_map");
            }
        }

        void visit(const arena::ast::CallExpression *node) override {
            auto &resolved_callee = current_expr->children[0];
            auto func_info = std::get_if<arena::sema::ResolvedFunctionInfo>(&resolved_callee.info);
            if (func_info == nullptr) {
                throw std::runtime_error("Expected function info for call expression");
            }

            auto func = ftable->get_function(func_info->function_id);
            if (!func.has_value()) {
                throw std::runtime_error("Function not found in function table");
            }
            auto func_type = getLLVMFunctionType(*func);

            std::vector<::llvm::Value *> args;
            for (int i = 1; i < current_expr->num_children; ++i) {
                visitExpression(&current_expr->children[i]);
                args.push_back(current_value);
            }

            auto callee = module->getFunction(func->get_symbol().name);
            if (!callee) {
                throw std::runtime_error("Callee function not found in module");
            }

            current_value = builder->CreateCall(callee, args, "calltmp");
        }
    };

    void output_ir_to_file(::llvm::Module &module, const std::string &output_path) {
        auto targetTriple = ::llvm::sys::getDefaultTargetTriple();
        ::llvm::InitializeNativeTarget();
        ::llvm::InitializeNativeTargetAsmPrinter();

        std::string error;
        auto target = ::llvm::TargetRegistry::lookupTarget(targetTriple, error);
        if (!target) {
            throw std::runtime_error("Failed to lookup target: " + error);
        }

        auto cpu = "generic";
        auto features = "";
        ::llvm::TargetOptions opt;
        auto target_machine = target->createTargetMachine(targetTriple, cpu, features, opt, ::llvm::Reloc::Model::PIC_);
        module.setDataLayout(target_machine->createDataLayout());
        module.setTargetTriple(targetTriple);

        std::error_code ec;
        ::llvm::raw_fd_ostream fd_os(output_path, ec);

        if (ec) {
            throw std::runtime_error("Failed to open output file: " + ec.message());
        }

        ::llvm::legacy::PassManager pass_manager;
        auto object_file_type = ::llvm::CodeGenFileType::ObjectFile;
        if (target_machine->addPassesToEmitFile(pass_manager, fd_os, nullptr, object_file_type)) {
            throw std::runtime_error("Target machine can't emit a file of this type");
        }
        pass_manager.run(module);
        fd_os.flush();
    }

} // namespace

void arena::backend::emit_impl(const std::vector<arena::backend::ResolvedCompilationUnit> &resolved,
                               const arena::backend::BackendOptions &options) {
    std::string module_name;
    for (const auto &unit : resolved) {
        module_name += unit.source_path.stem().string();
    }

    ::llvm::LLVMContext context;
    ::llvm::Module module(module_name, context);
    ::llvm::IRBuilder<> builder(context);

    for (const auto &unit : resolved) {
        auto ftable = unit.ftable;
        auto ttable = unit.ttable;
        for (const auto decl : unit.resolved->get_resolved_decls()) {
            LLVMBackendAstVisitor visitor(context, module, builder, decl, ftable, ttable);
            // TODO: don't require visiting the original AST node directly
            decl->original->accept(&visitor);
        }
    }

    if (options.print_ir) {
        ::llvm::outs() << "LLVM IR:\n";
        module.print(::llvm::outs(), nullptr);
    }

    if (options.validate_ir) {
        ::llvm::verifyModule(module, &::llvm::errs());
        ::llvm::outs() << "IR validation successful.\n";
    }

    output_ir_to_file(module, options.output_path.string());
}
