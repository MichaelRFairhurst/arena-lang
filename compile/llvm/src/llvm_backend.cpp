#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Transforms/IPO/InferFunctionAttrs.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
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
                              const arena::sema::TypeTable *ttable,
                              const arena::sema::VariableRegistry *variables)
            : context(&context), module(&module), builder(&builder), current_decl(current_decl),
              ftable(ftable), ttable(ttable), variables(variables) {}

    private:
        struct InMemoryValue {
            llvm::Value *alloca = nullptr;
            size_t alignment = 0;
            ::llvm::Type *type = nullptr;
            std::string_view name;
        };

        struct CurrentValue {
            llvm::Value *reg = nullptr;
            std::optional<InMemoryValue> mem = std::nullopt;
        };

        llvm::LLVMContext *context;
        llvm::Module *module;
        llvm::IRBuilder<> *builder;
        llvm::Function *current_function = nullptr;
        const arena::sema::ResolvedExpression *current_expr = nullptr;
        const arena::sema::ResolvedDeclaration *current_decl;
        CurrentValue current_value;
        bool has_exited = false;
        const arena::sema::FunctionTable *ftable;
        const arena::sema::TypeTable *ttable;
        const arena::sema::VariableRegistry *variables;
        std::unordered_map<arena::sema::VariableId, InMemoryValue> variable_map;

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

        std::pair<llvm::Function *, const arena::sema::ResolvedFunctionDeclaration *>
        declare_function(const arena::ast::FunctionDeclaration *node) {
            auto func_info =
                std::get_if<arena::sema::ResolvedFunctionDeclaration>(&current_decl->info);
            if (!func_info) {
                throw std::runtime_error("Failed to get function info from resolved declaration");
            }

            if (!func_info->return_type.has_value()) {
                throw std::runtime_error(
                    "Function must have a return type (explicit void required if none)");
            }

            llvm::Type *return_type = getLLVMType(func_info->return_type.value());
            std::vector<llvm::Type *> param_types;
            std::vector<std::string_view> param_names;
            for (int i = 0; i < func_info->num_parameters; ++i) {
                auto variable_id = func_info->parameters[i];
                auto param_var = variables->resolve_variable(variable_id);
                if (!param_var->has_type()) {
                    throw std::runtime_error("Cannot emit llvm for parameter without a type");
                }

                param_types.push_back(getLLVMType(*param_var->get_type_id()));
                param_names.push_back(param_var->name);
            }

            auto func_type = llvm::FunctionType::get(return_type, param_types, false);

            llvm::Function *function = llvm::Function::Create(func_type,
                                                              llvm::Function::ExternalLinkage,
                                                              node->get_name(),
                                                              *module);

            auto args = function->arg_begin();
            for (auto name : param_names) {
                args->setName(name);
                ++args;
            }

            return {function, func_info};
        }

        void visit(const arena::ast::FunctionDeclaration *node) override { declare_function(node); }

        void visit(const arena::ast::StructDefinition *node) override {
            auto struct_info =
                std::get_if<arena::sema::ResolvedStructDeclaration>(&current_decl->info);

            if (!struct_info) {
                throw std::runtime_error("Struct info not available");
            }

            std::vector<::llvm::Type *> members;
            for (size_t i = 0; i < struct_info->num_members; ++i) {
                auto member_type_id = struct_info->members[i].type_id;
                members.push_back(getLLVMType(member_type_id));
            }

            auto type = ::llvm::StructType::create(*context, members, node->get_name());
        }

        void visit(const arena::ast::FunctionDefinition *node) override {
            auto [llvm_function, func_info] = declare_function(node);
            current_function = llvm_function;

            // Create a basic block and set the insertion point
            llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", current_function);
            builder->SetInsertPoint(entry);

            auto args = current_function->arg_begin();
            for (int i = 0; i < func_info->num_parameters; ++i) {
                auto variable_id = func_info->parameters[i];
                auto param_var = variables->resolve_variable(variable_id);

                auto stack_var = InMemoryValue{};
                stack_var.alignment = 1; // TODO: Determine proper alignment based on type
                stack_var.alloca = builder->CreateAlloca(args->getType(), nullptr, param_var->name);
                stack_var.type = args->getType();
                stack_var.name = param_var->name;
                variable_map[variable_id] = stack_var;

                builder->CreateStore(&*args, stack_var.alloca);
                ++args;
            }

            visitStatement(current_decl->resolved_stmt);

            if (current_function->getReturnType()->isVoidTy() && !has_exited) {
                builder->CreateRetVoid();
            }
        }

        void visitExpression(const arena::sema::ResolvedExpression *node) {
            auto prev_expr = current_expr;
            current_expr = node;
            node->original->accept(this);
            current_expr = prev_expr;
        }

        void set_current_reg(::llvm::Value *value) {
            current_value.reg = value;
            current_value.mem.reset();
        }

        ::llvm::Value *read_current_value() {
            if (current_value.mem.has_value()) {
                return builder->CreateAlignedLoad(current_value.mem->type,
                                                  current_value.mem->alloca,
                                                  ::llvm::MaybeAlign{current_value.mem->alignment},
                                                  ::llvm::Twine{"loadtmp"} +
                                                      current_value.mem->name);
            } else {
                return current_value.reg;
            }
        }

        class ResolvedStatementVisitor {
        public:
            ResolvedStatementVisitor(llvm::LLVMContext *context,
                                     llvm::IRBuilder<> *builder,
                                     llvm::Function *current_function,
                                     LLVMBackendAstVisitor *visitor)
                : context(context), builder(builder), current_function(current_function),
                  visitor(visitor) {}

            void operator()(const arena::sema::ResolvedIfStatement &resolved_stmt) {
                if (visitor->has_exited) {
                    return;
                }

                visitor->visitExpression(resolved_stmt.condition);
                auto true_block = llvm::BasicBlock::Create(*context, "true_block");
                auto false_block = llvm::BasicBlock::Create(*context, "false_block");
                auto merge_block = llvm::BasicBlock::Create(*context, "merge_block");

                builder->CreateCondBr(visitor->read_current_value(), true_block, false_block);

                current_function->insert(current_function->end(), true_block);
                builder->SetInsertPoint(true_block);
                visitor->visitStatement(resolved_stmt.then_branch);
                bool then_exits = visitor->has_exited;
                if (!then_exits) {
                    builder->CreateBr(merge_block);
                }

                current_function->insert(current_function->end(), false_block);
                builder->SetInsertPoint(false_block);
                visitor->has_exited = false;
                bool else_exits = false;
                if (resolved_stmt.else_branch) {
                    visitor->visitStatement(resolved_stmt.else_branch);
                    else_exits = visitor->has_exited;
                }
                if (!else_exits) {
                    builder->CreateBr(merge_block);
                }

                visitor->has_exited = then_exits && else_exits;
                if (!visitor->has_exited) {
                    current_function->insert(current_function->end(), merge_block);
                    builder->SetInsertPoint(merge_block);
                }
            }

            void operator()(const arena::sema::ResolvedLetStatement &resolved_stmt) {
                if (visitor->has_exited) {
                    return;
                }

                auto variable_id = resolved_stmt.variable_id;
                auto variable = visitor->variables->resolve_variable(variable_id);
                // TODO: get the type from the variable declaration instead of the initializer
                if (!variable->has_type()) {
                    throw std::runtime_error("Variable does not have a type");
                }

                auto type = visitor->getLLVMType(variable->get_type_id().value());

                auto stack_var = InMemoryValue{};
                // TODO: get the variable name
                stack_var.alloca = builder->CreateAlloca(type, nullptr, variable->name);
                stack_var.alignment = 1; // TODO: Determine proper alignment based on type
                stack_var.type = type;
                stack_var.name = variable->name;
                visitor->variable_map[variable_id] = stack_var;

                if (resolved_stmt.initializer != nullptr) {
                    visitor->visitExpression(resolved_stmt.initializer);
                    auto value = visitor->read_current_value();
                    builder->CreateStore(value, stack_var.alloca);
                }
            }

            void operator()(const arena::sema::ResolvedReturnStatement &resolved_stmt) {
                if (visitor->has_exited) {
                    return;
                }

                if (resolved_stmt.expr == nullptr) {
                    builder->CreateRetVoid();
                } else {
                    visitor->visitExpression(resolved_stmt.expr);
                    builder->CreateRet(visitor->read_current_value());
                }

                visitor->has_exited = true;
            }

            void operator()(const arena::sema::ResolvedBlockStatement &resolved_stmt) {
                for (size_t i = 0; i < resolved_stmt.num_statements; ++i) {
                    if (visitor->has_exited) {
                        return;
                    }

                    visitor->visitStatement(&resolved_stmt.statements[i]);
                }
            }

            void operator()(const arena::sema::ResolvedArenaStatement &resolved_stmt) {
                if (visitor->has_exited) {
                    return;
                }

                throw std::runtime_error("ResolvedArenaStatement not implemented");
            }

            void operator()(const arena::sema::ResolvedExprStatement &resolved_stmt) {
                if (visitor->has_exited) {
                    return;
                }

                visitor->visitExpression(resolved_stmt.expr);
            }

        private:
            llvm::LLVMContext *context;
            llvm::IRBuilder<> *builder;
            llvm::Function *current_function;
            LLVMBackendAstVisitor *visitor;
        };

        void visitStatement(arena::sema::ResolvedStatement *node) {
            std::visit(ResolvedStatementVisitor{context, builder, current_function, this},
                       node->info);
        }

        void visit(const arena::ast::StringLiteral *node) override {
            throw std::runtime_error("StringLiteral not implemented");
        }

        void visit(const arena::ast::IntegerLiteral *node) override {
            throw std::runtime_error("IntegerLiteral not implemented");
        }

        void visit(const arena::ast::Literal *node) override {
            auto value = node->begin()->literalValue;
            if (auto int_value = std::get_if<__int128_t>(&value)) {
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

                uint64_t value64 = integral->is_signed
                                       ? static_cast<uint64_t>(static_cast<int64_t>(*int_value))
                                       : static_cast<uint64_t>(*int_value);

                auto apint = ::llvm::APInt(integral->size_bits, *int_value, integral->is_signed);
                set_current_reg(llvm::ConstantInt::get(*context, apint));
            } else if (auto float_value = std::get_if<double>(&value)) {
                auto type_info = current_expr->type;
                if (!type_info.has_value()) {
                    throw std::runtime_error("Expected type information for float literal");
                }

                auto llvm_type = getLLVMType(type_info->type_id);
                set_current_reg(::llvm::ConstantFP::get(llvm_type, *float_value));
            } else if (auto string_value = std::get_if<std::string_view>(&value)) {
                set_current_reg(builder->CreateGlobalStringPtr(*string_value));
            } else if (node->begin()->type == arena::ast::TokenType::TRUE) {
                set_current_reg(llvm::ConstantInt::get(*context, llvm::APInt(1, 1)));
            } else if (node->begin()->type == arena::ast::TokenType::FALSE) {
                set_current_reg(llvm::ConstantInt::get(*context, llvm::APInt(1, 0)));
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
            if (it != variable_map.end()) {
                current_value.mem = it->second;
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

            if (func_type->getReturnType()->isVoidTy()) {
                builder->CreateCall(callee, args);
                set_current_reg(nullptr);
            } else {
                set_current_reg(builder->CreateCall(callee, args, "calltmp"));
            }
        }

        void visit(const arena::ast::BinaryExpression *node) override {
            auto &lhs_info = current_expr->children[0].type;
            if (!lhs_info.has_value()) {
                throw std::runtime_error("Binary expression has no type information");
            }

            auto lhs_type = ttable->get_type(lhs_info->type_id, &this->current_decl->lifetimes);

            auto integral_type =
                std::get_if<arena::sema::IntegralType>(&lhs_type.get_program_type());
            auto floating_type =
                std::get_if<arena::sema::FloatingType>(&lhs_type.get_program_type());

            if (node->get_operator() == arena::ast::TokenType::EQUAL) {
                visitExpression(&current_expr->children[0]);
                auto lhs = current_value.mem;
                if (!lhs.has_value()) {
                    throw std::runtime_error(
                        "Left-hand side of assignment is not a valid memory location");
                }
                visitExpression(&current_expr->children[1]);
                auto rhs = read_current_value();
                builder->CreateStore(rhs, lhs->alloca);
                return;
            }

            visitExpression(&current_expr->children[0]);
            auto lhs = read_current_value();
            visitExpression(&current_expr->children[1]);
            auto rhs = read_current_value();

            switch (node->get_operator()) {
            case arena::ast::TokenType::PLUS:
                if (integral_type != nullptr) {
                    set_current_reg(builder->CreateAdd(lhs, rhs, "addtmp"));
                } else if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFAdd(lhs, rhs, "addtmp"));
                } else {
                    throw std::runtime_error("Unsupported addition operation for given types");
                }
                break;
            case arena::ast::TokenType::MINUS:
                if (integral_type != nullptr) {
                    set_current_reg(builder->CreateSub(lhs, rhs, "subtmp"));
                } else if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFSub(lhs, rhs, "subtmp"));
                }
                break;
            case arena::ast::TokenType::STAR:
                if (integral_type != nullptr) {
                    set_current_reg(builder->CreateMul(lhs, rhs, "multmp"));
                } else if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFMul(lhs, rhs, "multmp"));
                }
                break;
            case arena::ast::TokenType::SLASH:
                if (integral_type != nullptr && integral_type->is_signed) {
                    set_current_reg(builder->CreateSDiv(lhs, rhs, "divtmp"));
                } else if (integral_type != nullptr) {
                    set_current_reg(builder->CreateUDiv(lhs, rhs, "divtmp"));
                } else if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFDiv(lhs, rhs, "divtmp"));
                } else {
                    throw std::runtime_error("Unsupported division operation for given types");
                }
                break;
            case arena::ast::TokenType::EQUAL_EQUAL:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpOEQ(lhs, rhs, "eqtmp"));
                } else {
                    set_current_reg(builder->CreateICmpEQ(lhs, rhs, "eqtmp"));
                }
                break;
            case arena::ast::TokenType::NOT_EQUAL:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpONE(lhs, rhs, "netmp"));
                } else {
                    set_current_reg(builder->CreateICmpNE(lhs, rhs, "netmp"));
                }
                break;
            case arena::ast::TokenType::GREATER:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpOGT(lhs, rhs, "gttmp"));
                } else {
                    set_current_reg(builder->CreateICmpSGT(lhs, rhs, "gttmp"));
                }
                break;
            case arena::ast::TokenType::LESS:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpOLT(lhs, rhs, "lttmp"));
                } else {
                    set_current_reg(builder->CreateICmpSLT(lhs, rhs, "lttmp"));
                }
                break;
            case arena::ast::TokenType::GREATER_EQUAL:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpOGE(lhs, rhs, "getmp"));
                } else {
                    set_current_reg(builder->CreateICmpSGE(lhs, rhs, "getmp"));
                }
                break;
            case arena::ast::TokenType::LESS_EQUAL:
                if (floating_type != nullptr) {
                    set_current_reg(builder->CreateFCmpOLE(lhs, rhs, "letmp"));
                } else {
                    set_current_reg(builder->CreateICmpSLE(lhs, rhs, "letmp"));
                }
                break;
            default:
                throw std::runtime_error("Unsupported binary operator");
            }
        }

        void visit(const arena::ast::DotOperatorExpression *node) override {
            switch (node->get_operator()) {
            case arena::ast::TokenType::AMP: {
                visitExpression(&current_expr->children[0]);
                if (!current_value.mem) {
                    // TODO: This will need to properly handle addressing members etc.
                    throw std::runtime_error("Unsupported address-of operation");
                }

                set_current_reg(current_value.mem->alloca);
                return;
            }

            case arena::ast::TokenType::STAR: {
                visitExpression(&current_expr->children[0]);
                auto operand_type_info = current_expr->children[0].type;

                if (!operand_type_info) {
                    throw std::runtime_error(
                        "Dot operator used on an expression with no type information");
                }

                auto type = ttable->get_type(operand_type_info->type_id, &current_decl->lifetimes);
                auto pointer_type = std::get_if<arena::sema::PointerType>(&type.get_program_type());

                if (!pointer_type) {
                    throw std::runtime_error("Dot operator used on a non-pointer type");
                }

                auto pointee_type = getLLVMType(pointer_type->pointee_type);
                // If .* produces an l-value, our "current value" after visiting this node
                // should be an l-value representing the address of the pointee.
                ::llvm::Value *dereferenced_value = nullptr;
                if (current_value.mem) {
                    // If our "current value" backed by memory, we have to find the value of
                    // that memory.
                    dereferenced_value =
                        builder->CreateLoad(builder->getPtrTy(), current_value.mem->alloca);
                } else {
                    // If our "current value" is in a register, we want to use that value which
                    // points to the lvalue.
                    dereferenced_value = current_value.reg;
                }

                // Either way, we now want the current value to represent the pointed-to
                // address found above.
                current_value.reg = nullptr;
                current_value.mem = InMemoryValue{};
                current_value.mem->alloca = dereferenced_value;
                current_value.mem->name = "deref_tmp";
                current_value.mem->alignment = 1; // TODO: get actual alignment
                current_value.mem->type = pointee_type;
                return;
            }

            default:
                throw std::runtime_error("Unsupported dot operator");
            }
        }

        void visit(const arena::ast::UnaryPrefixExpression *ast) override {
            if (ast->get_operator() == arena::ast::TokenType::MINUS) {
                visitExpression(&current_expr->children[0]);
                auto type_info = current_expr->type;
                if (!type_info.has_value()) {
                    throw std::runtime_error("Unary prefix expression has no type information");
                }

                auto type = ttable->get_type(type_info->type_id, &this->current_decl->lifetimes);

                if (std::holds_alternative<arena::sema::IntegralType>(type.get_program_type())) {
                    auto val = read_current_value();
                    auto zero = builder->getIntN(val->getType()->getIntegerBitWidth(), 0);
                    set_current_reg(builder->CreateSub(zero, val));
                } else if (std::holds_alternative<arena::sema::FloatingType>(
                               type.get_program_type())) {
                    set_current_reg(builder->CreateFNeg(read_current_value()));
                } else {
                    throw std::runtime_error("Unsupported type for unary prefix minus");
                }
            } else {
                throw std::runtime_error("Unsupported unary prefix operator");
            }
        }
    };

    void optimize(::llvm::Module &module, arena::backend::OptimizationLevel level) {
        ::llvm::LoopAnalysisManager loop_analysis_manager;
        ::llvm::FunctionAnalysisManager func_analysis_manager;
        ::llvm::CGSCCAnalysisManager cg_analysis_manager;
        ::llvm::ModuleAnalysisManager mod_analysis_manager;

        ::llvm::PassBuilder pass_builder;
        pass_builder.registerModuleAnalyses(mod_analysis_manager);
        pass_builder.registerCGSCCAnalyses(cg_analysis_manager);
        pass_builder.registerLoopAnalyses(loop_analysis_manager);
        pass_builder.registerFunctionAnalyses(func_analysis_manager);
        pass_builder.crossRegisterProxies(loop_analysis_manager,
                                          func_analysis_manager,
                                          cg_analysis_manager,
                                          mod_analysis_manager);

        // TODO: Consider writing our own optimization pipeline tailored to Arena's needs.
        ::llvm::OptimizationLevel opt_llvm;
        switch (level) {
        case arena::backend::OptimizationLevel::None:
            // Note that LLVM does define an optimization pass for O0. For example, __always_inline
            // functions will still be inlined.
            opt_llvm = ::llvm::OptimizationLevel::O0;
            break;
        case arena::backend::OptimizationLevel::Debug:
            opt_llvm = ::llvm::OptimizationLevel::O1;
            break;
        case arena::backend::OptimizationLevel::Performance:
            opt_llvm = ::llvm::OptimizationLevel::O2;
            break;
        case arena::backend::OptimizationLevel::Aggressive:
            opt_llvm = ::llvm::OptimizationLevel::O3;
            break;
        case arena::backend::OptimizationLevel::Size:
            opt_llvm = ::llvm::OptimizationLevel::Os;
            break;
        case arena::backend::OptimizationLevel::Minify:
            opt_llvm = ::llvm::OptimizationLevel::Oz;
            break;
        }

        auto mod_pass_manager = pass_builder.buildPerModuleDefaultPipeline(opt_llvm);
        mod_pass_manager.run(module, mod_analysis_manager);
    }

    ::llvm::TargetMachine *set_target(::llvm::Module &module) {
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
        return target_machine;
    }

    void output_ir_to_file(::llvm::Module &module,
                           ::llvm::TargetMachine *target_machine,
                           const std::string &output_path) {
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
        auto vars = unit.resolved->get_resolved_variables();
        for (const auto decl : unit.resolved->get_resolved_decls()) {
            LLVMBackendAstVisitor visitor(context, module, builder, decl, ftable, ttable, vars);
            // TODO: don't require visiting the original AST node directly
            decl->original->accept(&visitor);
        }
    }

    if (options.validate_ir) {
        if (::llvm::verifyModule(module, &::llvm::errs())) {
            ::llvm::outs() << "IR validation failed.\n";
            ::llvm::outs() << "LLVM IR:\n";
            module.print(::llvm::outs(), nullptr);

            return;
        }

        ::llvm::outs() << "IR validation successful.\n";
    }

    auto target_machine = set_target(module);
    optimize(module, options.optimization_level);

    if (options.print_ir) {
        ::llvm::outs() << "LLVM IR:\n";
        module.print(::llvm::outs(), nullptr);
    }

    output_ir_to_file(module, target_machine, options.output_path.string());
}
