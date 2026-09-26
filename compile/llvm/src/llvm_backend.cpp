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
        struct StackValue {
            llvm::Value *alloca = nullptr;
            size_t alignment = 0;
            ::llvm::Type *type = nullptr;
            const char *name;
        };

        struct CurrentValue {
            llvm::Value *reg = nullptr;
            StackValue *mem = nullptr;
        };

        llvm::LLVMContext *context;
        llvm::Module *module;
        llvm::IRBuilder<> *builder;
        const arena::sema::ResolvedExpression *current_expr = nullptr;
        const arena::sema::ResolvedDeclaration *current_decl;
        CurrentValue current_value;
        const arena::sema::FunctionTable *ftable;
        const arena::sema::TypeTable *ttable;
        std::unordered_map<arena::sema::VariableId, StackValue> variable_map;

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
            auto func_info =
                std::get_if<arena::sema::ResolvedFunctionDeclaration>(&current_decl->info);
            if (!func_info) {
                throw std::runtime_error("Failed to get function info from resolved declaration");
            }

            auto args = function->arg_begin();

            // Create a basic block and set the insertion point
            llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", function);
            builder->SetInsertPoint(entry);

            for (int i = 0; i < func_info->num_parameters; ++i) {
                auto variable_id = func_info->parameters[i];
                // TODO: get variable name

                auto &stack_var = variable_map[variable_id];
                stack_var.alignment = 1; // TODO: Determine proper alignment based on type
                stack_var.alloca = builder->CreateAlloca(args->getType(), nullptr, args->getName());
                stack_var.type = args->getType();
                stack_var.name = "arg"; // TODO: get variable name

                builder->CreateStore(&*args, stack_var.alloca);
                ++args;
            }

            visitStatement(current_decl->resolved_stmt);
        }

        void visitExpression(const arena::sema::ResolvedExpression *node) {
            auto prev_expr = current_expr;
            current_expr = node;
            node->original->accept(this);
            current_expr = prev_expr;
        }

        void set_current_reg(::llvm::Value *value) {
            current_value.reg = value;
            current_value.mem = nullptr;
        }

        ::llvm::Value *read_current_value() {
            if (current_value.mem != nullptr) {
                return builder->CreateAlignedLoad(current_value.mem->type,
                                                  current_value.mem->alloca,
                                                  ::llvm::MaybeAlign{current_value.mem->alignment},
                                                  ::llvm::Twine{"loadtmp"} + current_value.mem->name);
            } else {
                return current_value.reg;
            }
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

                builder->CreateCondBr(visitor->read_current_value(), true_block, false_block);

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
                if (resolved_stmt.initializer == nullptr) {
                    throw std::runtime_error("Not yet implemented: let without initializer.");
                }

                auto variable_id = resolved_stmt.variable_id;
                // TODO: get the type from the variable declaration instead of the initializer
                auto type = visitor->getLLVMType(resolved_stmt.initializer->type->type_id);

                auto &stack_var = visitor->variable_map[variable_id];
                // TODO: get the variable name
                stack_var.alloca = builder->CreateAlloca(type, nullptr, "alloca_tmp");
                stack_var.alignment = 1; // TODO: Determine proper alignment based on type
                stack_var.type = type;
                stack_var.name = {"alloca_tmp"};

                visitor->visitExpression(resolved_stmt.initializer);
                auto value = visitor->read_current_value();
                builder->CreateStore(value, stack_var.alloca);
            }

            void operator()(const arena::sema::ResolvedReturnStatement &resolved_stmt) {
                visitor->visitExpression(resolved_stmt.expr);
                builder->CreateRet(visitor->read_current_value());
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
                auto type_info = current_expr->type;
                if (!type_info.has_value()) {
                    throw std::runtime_error("Expected type information for integer literal");
                }

                auto type =
                    ttable->get_type(type_info.value().type_id, &this->current_decl->lifetimes);
                auto integral = std::get_if<arena::sema::IntegralType>(&type.get_program_type());

                if (!integral) {
                    throw std::runtime_error("Expected integral type for integer literal");
                }

                auto apint =
                    ::llvm::APInt(integral->size_bytes * 8, *int_value, integral->is_signed);
                set_current_reg(llvm::ConstantInt::get(*context, apint));
            } else if (auto string_value = std::get_if<std::string_view>(&value)) {
                set_current_reg(builder->CreateGlobalStringPtr(*string_value));
            } else if (node->begin()->type == arena::ast::TokenType::TRUE) {
                // TODO: use a size of 1 bit
                set_current_reg(llvm::ConstantInt::get(*context, llvm::APInt(8, 1)));
            } else if (node->begin()->type == arena::ast::TokenType::FALSE) {
                // TODO: use a size of 1 bit
                set_current_reg(llvm::ConstantInt::get(*context, llvm::APInt(8, 0)));
            } else {
                throw std::runtime_error("Expected integer or string literal");
            }
        }

        void visit(const arena::ast::LiteralExpression *node) override {
            node->get_literal()->accept(this);
        }

        void visit(const arena::ast::IdExpression *node) override {
            auto info = std::get_if<arena::sema::ResolvedVariableInfo>(&current_expr->info);
            if (!info) {
                throw std::runtime_error("Expected variable info for id expression");
            }

            auto it = variable_map.find(info->variable_id);
            ::llvm::errs() << "Looking up variable_id " << info->variable_id.v_id
                           << " in variable_map\n";
            if (it != variable_map.end()) {
                current_value.mem = &it->second;
                current_value.reg = nullptr;
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
                args.push_back(read_current_value());
            }

            auto callee = module->getFunction(func->get_symbol().name);
            if (!callee) {
                throw std::runtime_error("Callee function not found in module");
            }

            set_current_reg(builder->CreateCall(callee, args, "calltmp"));
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
        auto target_machine = target->createTargetMachine(targetTriple,
                                                          cpu,
                                                          features,
                                                          opt,
                                                          ::llvm::Reloc::Model::PIC_);
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
        if (::llvm::verifyModule(module, &::llvm::errs())) {
            ::llvm::outs() << "IR validation failed.\n";
            return;
        }

        ::llvm::outs() << "IR validation successful.\n";
    }

    output_ir_to_file(module, options.output_path.string());
}
