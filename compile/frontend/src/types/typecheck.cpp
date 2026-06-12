#include "types/typecheck.hpp"
#include "types/inference.hpp"
#include "types/lifetimes.hpp"
#include "types/substitute.hpp"
#include "errors/errors.hpp"
#include <iostream>

namespace {
    using namespace arena;
    using namespace arena::sema;

    class TypeCheckTransform {
    public:
        TypeCheckTransform(util::Arena *arena, TypecheckOperations ops)
            : middleware(arena), ops(ops) {}

        ResolvedExpression *typecheck_root(const ResolvedExpression *expr_in,
                                           InferenceContext *inference_ctx = nullptr) {
            InferenceContext ctx{expr_in->original, &ops};
            if (inference_ctx == nullptr) {
                inference_ctx = &ctx;
            }

            this->inference_ctx = inference_ctx;
            auto expr = middleware.transform_root(*expr_in, *this);
            this->inference_ctx = nullptr;
            return expr;
        }
        TypeId set_resolved_info(ResolvedExpressionInfo &info,
                                 ResolvedValueCategory value_category) {
            auto type_id = inference_ctx->get_inferred_context_type();
            info = ResolvedTypeInfo{type_id, value_category};
            return type_id;
        }

        TypeId set_type(ResolvedExpression *expr,
                        TypeId type_id,
                        ResolvedValueCategory value_category) {
            expr->info = ResolvedTypeInfo{type_id, value_category};
            inference_ctx->constrain_context_type(type_id, error::Link(expr->original));
            return type_id;
        }

        TypeId set_type(ResolvedExpression *expr,
                        TypeSymbol symbol,
                        ResolvedValueCategory value_category) {
            auto type_id = ops.get_types().get_type_id(symbol);
            return set_type(expr, type_id, value_category);
        }

        template <typename Ast>
        InferenceContext make_child_context(ExprTransformStep<Ast> step, size_t child_index) {
            auto node = step.original->children[child_index].original;
            return InferenceContext(node, &ops);
        }

        template <typename Ast>
        TypeId resolve_child(ExprTransformStep<Ast> step,
                             size_t child_index,
                             InferenceContext *ctx) {
            auto prev_ctx = inference_ctx;
            inference_ctx = ctx;
            auto child_type_id = middleware.transform_child<TypeId>(step, child_index, *this);
            inference_ctx = prev_ctx;
            return child_type_id;
        }

        TypeId operator()(ExprTransformStep<ast::IdExpression> step) {
            auto var_info = std::get_if<ResolvedVariableInfo>(&step.original_info());
            if (!var_info) {
                inference_ctx->constrain_context_type(ops.get_types().get_type_id(
                                                          ErrorTypeSymbol{}),
                                                      error::Link{step.original->original,
                                                                  "unknown identifier"});
            }

            auto variable = ops.get_variables().resolve_variable(var_info->variable_id);
            inference_ctx->constrain_context_type(var_info->variable_id);
            return set_resolved_info(step.out_info(), ResolvedLValue{variable->lifetime});
        }

        TypeId operator()(ExprTransformStep<ast::LiteralExpression> step) {
            // For now we treat all literals as ints...
            return set_type(step.out, NamedTypeSymbol{"int"}, ResolvedRValue{});
        }

        TypeId operator()(ExprTransformStep<ast::BinaryExpression> step) {
            if (step.ast->get_operator() == ast::TokenType::EQUAL) {
                return handle_assignment(step);
            }

            auto left_inference_ctx = make_child_context(step, 0);
            auto left_type_id = resolve_child(step, 0, &left_inference_ctx);

            auto right_inference_ctx = make_child_context(step, 1);
            auto right_type_id = resolve_child(step, 1, &right_inference_ctx);

            // We do not support many implicit conversions. For now, the types must match exactly,
            // or we produce an error type.
            if (!ops.types_equal(left_type_id, right_type_id)) {
                auto type_left = ops.get_type_name(left_type_id);
                auto type_right = ops.get_type_name(right_type_id);
                auto link_left = error::Link{step.ast->get_left(), std::string(type_left)};
                auto link_right = error::Link{step.ast->get_right(), std::string(type_right)};
                ops.get_errors()
                    .report(step.ast,
                            "Expected both binary operands to have matching types, but got ",
                            link_left,
                            " and ",
                            link_right);
            }

            switch (step.ast->get_operator()) {
            case ast::TokenType::EQUAL_EQUAL:
            case ast::TokenType::NOT_EQUAL:
            case ast::TokenType::LESS:
            case ast::TokenType::LESS_EQUAL:
            case ast::TokenType::GREATER:
            case ast::TokenType::GREATER_EQUAL:
                return set_type(step.out, NamedTypeSymbol{"bool"}, ResolvedRValue{});
            default:
                return set_type(step.out, left_type_id, ResolvedRValue{});
            }
        }

        TypeId handle_assignment(ExprTransformStep<ast::BinaryExpression> step) {
            auto left_inference_ctx = make_child_context(step, 0);
            auto left_type_id = resolve_child(step, 0, &left_inference_ctx);
            auto left_info = std::get<ResolvedTypeInfo>(step.out->children[0].info);
            if (!std::holds_alternative<ResolvedLValue>(left_info.value_category)) {
                ops.get_errors().report(step.ast, "Left-hand side of assignment must be an lvalue");
            }

            auto right_inference_ctx = make_child_context(step, 1);
            right_inference_ctx.constrain_context_type(left_type_id,
                                                       error::Link{step.ast, "assignment"});
            auto right_type_id = resolve_child(step, 1, &right_inference_ctx);

            ops.require_assignable(left_type_id,
                                   right_type_id,
                                   step.ast,
                                   "Type mismatch in assignment");

            inference_ctx->constrain_context_type(left_type_id,
                                                  error::Link{step.ast, "assignment"});
            return set_resolved_info(step.out_info(), ResolvedRValue{});
        }

        TypeId operator()(ExprTransformStep<ast::CallExpression> step) {
            auto callee = step.original->children[0];
            auto finfo = std::get_if<ResolvedFunctionInfo>(&callee.info);
            if (!finfo) {
                // For now, the callee must be a function. Soon we'll add function types.
                ops.get_errors().report(callee.original, "Expected a function in call expression");
                return set_type(step.out, ErrorTypeSymbol{}, ResolvedRValue{});
            }

            auto func = ops.get_functions().get_function(finfo->function_id);
            if (!func) {
                ops.get_errors().report(callee.original, "Unknown function in call expression");
                return set_type(step.out, ErrorTypeSymbol{}, ResolvedRValue{});
            }

            auto params = func->get_param_types();
            auto return_type = func->get_return_type();

            if (params->size() != step.original->num_children - 1) {
                ops.add_error_expected(params->size(),
                                       step.original->num_children - 1,
                                       step.ast,
                                       "Argument count mismatch in call expression");
                return set_type(step.out, ErrorTypeSymbol{}, ResolvedRValue{});
            }

            auto func_lifetimes = func->get_lifetimes();
            auto substitution_map = ops.get_lifetimes().import(func_lifetimes);

            for (size_t i = 1; i < step.original->num_children; ++i) {
                std::cout << "Original param type: " << ops.get_type_name(params->at(i - 1), func_lifetimes) << std::endl;
                auto param_type = ops.substitute_lifetimes(params->at(i - 1), func_lifetimes, substitution_map);
                std::cout << "Substituted param type: " << ops.get_type_name(param_type) << std::endl;
                auto arg_inference_ctx = make_child_context(step, i);
                // TODO: Check assignability rather than exact type equality.
                arg_inference_ctx.constrain_context_type(param_type,
                                                         error::Link(step.ast,
                                                                     "argument " +
                                                                         std::to_string(i)));
                auto arg_id = resolve_child(step, i, &arg_inference_ctx);
            }

            if (return_type) {
                std::cout << "Original return type: " << ops.get_type_name(*return_type, func_lifetimes) << std::endl;
                auto substituted_return_type = ops.substitute_lifetimes(*return_type, func_lifetimes, substitution_map);
                std::cout << "Substituted return type: " << ops.get_type_name(substituted_return_type) << std::endl;
                inference_ctx->constrain_context_type(substituted_return_type,
                                                      error::Link(step.ast, "return type"));
            } else {
                inference_ctx->constrain_context_type(ops.get_types().get_type_id(VoidTypeSymbol{}),
                                                      error::Link(step.ast, "return type"));
            }

            return set_resolved_info(step.out_info(), ResolvedRValue{});
        }

        TypeId operator()(ExprTransformStep<ast::DotOperatorExpression> step) {
            auto inference_ctx_parent = inference_ctx;
            auto operand_inference_ctx = make_child_context(step, 0);
            ResolvedValueCategory value_category = ResolvedRValue{};

            switch (step.ast->get_operator()) {
            case ast::TokenType::STAR: {
                // Set constraint to the "unsafe" lifetime, which is the lifetime that accepts all
                // lifetimes. This is equivalent to not constraining the lifetime.
                operand_inference_ctx.constrain_points_to(inference_ctx_parent,
                                                          ops.get_lifetimes().get_unsafe_lifetime(),
                                                          step.ast);
                resolve_child(step, 0, &operand_inference_ctx);
                inference_ctx_parent->constrain_dereferences(&operand_inference_ctx, step.ast);
                auto child_info = std::get<ResolvedTypeInfo>(step.out->children[0].info);
                auto ptr_type = ops.get_type(child_info.type_id);
                auto ptr = std::get_if<PointerType>(&ptr_type.get_program_type());
                if (ptr) {
                    // Note: `constrain_dereferences` already reports errors for non-pointer types.
                    value_category = ResolvedLValue{ptr->lifetime};
                }
                break;
            }

            case ast::TokenType::AMP: {
                operand_inference_ctx.constrain_dereferences(inference_ctx_parent, step.ast);
                resolve_child(step, 0, &operand_inference_ctx);
                auto child_info = std::get<ResolvedTypeInfo>(step.out->children[0].info);
                LifetimeId lifetime = ops.get_lifetimes().get_unsafe_lifetime();
                if (auto lvalue_info = std::get_if<ResolvedLValue>(&child_info.value_category)) {
                    lifetime = lvalue_info->lifetime;
                } else {
                    ops.get_errors().report(step.ast, "Cannot take reference of an rvalue");
                }

                inference_ctx_parent->constrain_points_to(&operand_inference_ctx,
                                                          lifetime,
                                                          step.ast);
                value_category = ResolvedRValue{};
                break;
            }

            default:
                throw std::runtime_error("Unexpected .? operator type: " +
                                         std::string(step.ast->get_operator_token()->text));
            }

            return set_resolved_info(step.out_info(), value_category);
        }

        TypeId operator()(ExprTransformStep<ast::CastExpression> step) {
            auto casted_type =
                ops.get_types().get_type(step.ast->get_type(), &ops.get_lifetimes()).get_id();
            auto uncast_inference_ctx = make_child_context(step, 0);
            inference_ctx->constrain_context_type(casted_type,
                                                  error::Link(step.ast, "result of cast"));
            auto operand_type = resolve_child(step, 0, &uncast_inference_ctx);
            // TODO: check that cast is valid (e.g., cannot cast bool to struct)

            return set_resolved_info(step.out_info(), ResolvedRValue{});
        }

        TypeId operator()(ExprTransformStep<ast::MemberAccessExpression> step) {
            // For now we do not support member access, so this is always an error.
            ops.get_errors().report(step.ast, "Member access is not supported yet");
            return set_type(step.out, ErrorTypeSymbol{}, ResolvedRValue{});
        }

        TypeId operator()(ExprTransformStep<ast::UnaryPrefixExpression> step) {
            if (step.ast->get_operator() != ast::TokenType::NOT) {
                throw std::runtime_error("Unexpected unary prefix operator: " +
                                         std::string(step.ast->get_operator_token()->text));
            }

            auto bool_id = ops.get_types().get_type_id(NamedTypeSymbol{"bool"});
            auto operand_inference_ctx = make_child_context(step, 0);
            operand_inference_ctx.constrain_context_type(bool_id,
                                                         error::Link(step.ast, "operand of '!'"));
            inference_ctx->constrain_context_type(bool_id, error::Link(step.ast, "result of '!'"));
            resolve_child(step, 0, &operand_inference_ctx);
            return set_resolved_info(step.out_info(), ResolvedRValue{});
        }

    private:
        TypecheckOperations ops;
        ExprTransformMiddleware middleware;
        InferenceContext *inference_ctx = nullptr;
    };

    class StatementTypeCheckTransform {
    public:
        StatementTypeCheckTransform(util::Arena *arena,
                                    TypeCheckTransform *expr_typecheck,
                                    TypecheckOperations ops)
            : middleware(arena), expr_typecheck(expr_typecheck), ops(ops) {}

        ResolvedStatement *typecheck(const ResolvedStatement &stmt_in) {
            return middleware.transform_root(&stmt_in, *this);
        }

        void operator()(StmtTransformStep<ResolvedBlockStatement> step) {
            // By default, just copy the expression and recursively transform children
            for (size_t i = 0; i < step.original->num_statements; ++i) {
                middleware.transform_block_child(step, i, *this);
            }
        }

        void operator()(StmtTransformStep<ResolvedIfStatement> step) {
            step.out->condition = expr_typecheck->typecheck_root(step.original->condition);
            middleware.transform_if_then(step, *this);
            middleware.transform_if_else(step, *this);
        }

        void operator()(StmtTransformStep<ResolvedLetStatement> step) {
            step.out->variable_id = step.original->variable_id;
            if (!step.original->initializer) {
                return;
            }

            InferenceContext inference_ctx(step.original->initializer->original, &ops);

            auto variable = ops.get_variables().resolve_variable(step.out->variable_id);
            if (variable->has_type()) {
                inference_ctx.constrain_context_type(step.out->variable_id);
            }

            auto ast = step.original->initializer->original;
            auto initializer =
                expr_typecheck->typecheck_root(step.original->initializer, &inference_ctx);
            step.out->initializer = initializer;

            // We must do this to check for errors, and/or to infer the variable type.
            inference_ctx.constrain_context_type(step.out->variable_id);
        }

        void operator()(StmtTransformStep<ResolvedReturnStatement> step) {
            step.out->expr = expr_typecheck->typecheck_root(step.original->expr);
        }

        void operator()(StmtTransformStep<ResolvedExprStatement> step) {
            step.out->expr = expr_typecheck->typecheck_root(step.original->expr);
        }

    private:
        StmtTransformMiddleware middleware;
        TypeCheckTransform *expr_typecheck;
        TypecheckOperations ops;
    };

} // namespace

ResolvedType TypecheckOperations::get_type(TypeId id) const {
    return ttable->get_type(id, lifetimes);
}

std::string TypecheckOperations::get_type_name(TypeId id) const {
    return std::string(get_type(id).get_name());
}

std::string TypecheckOperations::get_type_name(TypeId id, const LifetimeGroup &type_lifetimes) const {
    return std::string(ttable->get_type(id, &type_lifetimes).get_name());
}

TypeId TypecheckOperations::substitute_lifetimes(
    TypeId type_id,
    const LifetimeGroup &type_lifetimes,
    const std::unordered_map<LifetimeId, LifetimeId> &substitutions) const {
    return arena::sema::substitute_lifetimes(type_id, type_lifetimes, ttable, substitutions);
}

void TypecheckOperations::add_error_expected(TypeId expected,
                                             TypeId actual,
                                             const ast::Node *node,
                                             std::string message) {
    errors->report(node,
                   message + ": expected '" + get_type_name(expected) + "', got '" +
                       get_type_name(actual) + "'");
}

void TypecheckOperations::add_error_expected(std::string_view expected,
                                             TypeId actual,
                                             const ast::Node *node,
                                             std::string message) {
    errors->report(node,
                   message + ": expected " + std::string(expected) + ", got '" +
                       get_type_name(actual) + "'");
}

void TypecheckOperations::add_error_expected(size_t num_expected,
                                             size_t num_actual,
                                             const ast::Node *node,
                                             std::string message) {
    errors->report(node,
                   message + ": expected '" + std::to_string(num_expected) + "', got '" +
                       std::to_string(num_actual) + "'");
}

void TypecheckOperations::require_assignable(
    TypeId lhs, TypeId rhs, const ast::Node *node, std::string message, bool force_strict) {
    if (lhs == rhs) {
        return;
    }

    auto lhs_type = get_type(lhs);
    auto rhs_type = get_type(rhs);
    if (lhs_type.is_error() || rhs_type.is_error()) {
        // Don't report spurious errors if one of the types is already an error
        return;
    }

    if (auto lhs_array = std::get_if<ArrayType>(&lhs_type.get_program_type())) {
        auto rhs_array = std::get_if<ArrayType>(&rhs_type.get_program_type());
        if (!rhs_array) {
            add_error_expected("a compatible array type", rhs, node, message);
        }

        if (rhs_array->size < lhs_array->size) {
            add_error_expected("an array of size at least " + std::to_string(lhs_array->size),
                               rhs,
                               node,
                               message);
        }

        require_assignable(lhs_type.get_id(), rhs_type.get_id(), node, message);
    } else if (auto left_ptr = std::get_if<PointerType>(&lhs_type.get_program_type())) {
        auto lhs_pointee_id = left_ptr->pointee_type;
        auto right_ptr = std::get_if<PointerType>(&rhs_type.get_program_type());
        if (!right_ptr) {
            add_error_expected("a pointer type in assignment", rhs, node, message);
            return;
        }
        auto rhs_pointee_id = right_ptr->pointee_type;
        auto rhs_pointee = get_type(rhs_pointee_id);
        auto constraint = rhs_pointee.is_lifetime_strict() || force_strict
                              ? LifetimeRelation::Equals
                              : LifetimeRelation::LessEqual;

        lifetimes->add_constraint(left_ptr->lifetime,
                                  constraint,
                                  right_ptr->lifetime,
                                  error::Link(node, "pointer assignment"));

        require_assignable(lhs_pointee_id, rhs_pointee_id, node, message, true);

        return;
    }

    add_error_expected(lhs, rhs, node, message);
}


void TypecheckOperations::require_type(TypeId expected,
                                       TypeId actual,
                                       const ast::Node *node,
                                       std::string message) {
    auto expected_type = get_type(expected);
    auto actual_type = get_type(actual);
    if (expected_type.is_error() || actual_type.is_error()) {
        // Don't report spurious errors if one of the types is already an error
        return;
    }

    if (expected != actual) {
        add_error_expected(expected, actual, node, message);
    }
}

bool TypecheckOperations::types_equal(TypeId a, TypeId b) {
    auto left_type = get_type(a);
    auto right_type = get_type(b);
    if (left_type.is_error() || right_type.is_error()) {
        // Don't report spurious errors if one of the types is already an error
        return true;
    }

    return a == b;
}


ResolvedExpressionsResult TypeChecker::type_check(
    const std::vector<const ResolvedDeclaration *> &decls, const VariableRegistry *variables) {
    util::Arena arena;
    std::vector<ResolvedDeclaration *> resolved_decls;
    error::Reporter errors;

    for (auto decl : decls) {
        if (!decl->resolved_stmt) {
            continue;
        }

        // Copy lifetime group
        auto lifetime_group = decl->lifetimes;

        TypecheckOperations ops(ftable, ttable, variables, &lifetime_group, &errors);
        TypeCheckTransform typecheck(&arena, ops);
        StatementTypeCheckTransform stmt_typecheck(&arena, &typecheck, ops);

        auto result = stmt_typecheck.typecheck(*decl->resolved_stmt);

        std::cout << lifetime_group.to_string() << std::endl;

        LifetimeSolver solver(&errors);
        solver.solve(lifetime_group);

        auto resolved = arena.alloc<ResolvedDeclaration>();
        resolved->original = decl->original;
        resolved->resolved_stmt = result;
        resolved->lifetimes = std::move(lifetime_group);
        resolved_decls.push_back(resolved);
    }

    return ResolvedExpressionsResult(std::move(arena),
                                     std::move(resolved_decls),
                                     errors.get_errors(),
                                     *variables // intentionally copied.
    );
}