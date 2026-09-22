#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
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

        llvm::Function *declare_function(const arena::ast::FunctionDeclaration *node) {
            // TODO: Don't require looking up the function via the function table; we should have
            // this information available directly from the resolved declaration.
            auto func = ftable->resolve(node->get_name());
            if (!func) {
                throw std::runtime_error("Function not found in function table: " +
                                         std::string(node->get_name()));
            }

            llvm::Type *return_type = getLLVMType(func->get_return_type().value_or(
                ttable->get_type_id(arena::sema::VoidTypeSymbol{})));

            std::vector<llvm::Type *> param_types;
            for (const auto &param_type_id : *func->get_param_types()) {
                param_types.push_back(getLLVMType(param_type_id));
            }

            llvm::FunctionType *func_type =
                llvm::FunctionType::get(return_type, param_types, false);

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
    };

} // namespace

std::string arena::backend::emit_impl(const arena::sema::ResolvedExpressionsResult &resolved,
                                      const arena::sema::FunctionTable &ftable,
                                      const arena::sema::TypeTable &ttable,
                                      const std::filesystem::path &source_path) {
    ::llvm::LLVMContext context;
    ::llvm::Module module(source_path.string(), context);

    // Create an IR builder
    ::llvm::IRBuilder<> builder(context);

    for (const auto decl : resolved.get_resolved_decls()) {
        LLVMBackendAstVisitor visitor(context, module, builder, decl, &ftable, &ttable);
        // TODO: don't require visiting the original AST node directly
        decl->original->accept(&visitor);
    }

    ::llvm::verifyModule(module, &::llvm::errs());
    std::string out;
    ::llvm::raw_string_ostream ros(out);
    module.print(ros, nullptr);
    return out;
}
