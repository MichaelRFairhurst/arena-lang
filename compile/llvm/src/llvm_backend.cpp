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
        const arena::sema::ResolvedDeclaration *current_decl;
        const arena::sema::FunctionTable *ftable;
        const arena::sema::TypeTable *ttable;

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

            // Create a basic block and set the insertion point
            llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", function);
            builder->SetInsertPoint(entry);

            // TODO: Set function arguments
            // auto args = addFunc->arg_begin();
            // llvm::Value *a = args++;
            // llvm::Value *b = args;
            // a->setName("a");
            // b->setName("b");

            // For demonstration, return a constant value (e.g., 42)
            builder->CreateRet(llvm::ConstantInt::get(builder->getInt32Ty(), 42));

            // Verify the function
            if (llvm::verifyFunction(*function, &llvm::errs())) {
                throw std::runtime_error("Function verification failed for: " +
                                         std::string(node->get_name()));
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

    std::string out;
    ::llvm::raw_string_ostream ros(out);
    module.print(ros, nullptr);
    return out;
}
