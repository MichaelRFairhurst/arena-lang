#include "types/typecheck.hpp"
#include "types/inference.hpp"
#include "errors/errors.hpp"
#include <iostream>

namespace {
    using namespace arena;
    using namespace arena::sema;

    class TypeCheckTransform {
    public:
        TypeCheckTransform(util::Arena *arena,
                           const FunctionTable *ftable,
                           const TypeTable *ttable,
                           const VariableRegistry *variables,
                           error::Reporter *errors)
            : middleware(arena), ftable(ftable), ttable(ttable), variables(variables),
              errors(errors) {}

        ResolvedExpression *typecheck_root(const ResolvedExpression *expr_in,
                                           InferenceContext *inference_ctx = nullptr) {
            InferenceContext ctx{variables, ttable, expr_in->original, errors};
            if (inference_ctx == nullptr) {
                inference_ctx = &ctx;
            }

            this->inference_ctx = inference_ctx;
            auto expr = middleware.transform_root(*expr_in, *this);
            this->inference_ctx = nullptr;
            return expr;
        }

        void add_error_expected(TypeId expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message) {
            errors->report(node,
                           message + ": expected '" +
                               std::string(ttable->get_type(expected).get_name()) + "', got '" +
                               std::string(ttable->get_type(actual).get_name()) + "'");
        }

        void add_error_expected(std::string_view expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message) {
            errors->report(node,
                           message + ": expected " + std::string(expected) + ", got '" +
                               std::string(ttable->get_type(actual).get_name()) + "'");
        }

        void add_error_expected(size_t num_expected,
                                size_t num_actual,
                                const ast::Node *node,
                                std::string message) {
            errors->report(node,
                           message + ": expected '" + std::to_string(num_expected) + "', got '" +
                               std::to_string(num_actual) + "'");
        }

        void require_type(TypeId expected,
                          TypeId actual,
                          const ast::Node *node,
                          std::string message) {
            auto expected_type = ttable->get_type(expected);
            auto actual_type = ttable->get_type(actual);
            if (expected_type.is_error() || actual_type.is_error()) {
                // Don't report spurious errors if one of the types is already an error
                return;
            }

            if (expected != actual) {
                add_error_expected(expected, actual, node, message);
            }
        }

        bool types_equal(TypeId a, TypeId b) {
            auto left_type = ttable->get_type(a);
            auto right_type = ttable->get_type(b);
            if (left_type.is_error() || right_type.is_error()) {
                // Don't report spurious errors if one of the types is already an error
                return true;
            }

            return a == b;
        }

        TypeId set_resolved_info(ResolvedExpressionInfo &info) {
            auto type_id = inference_ctx->get_inferred_context_type();
            info = ResolvedTypeInfo{type_id};
            return type_id;
        }

        TypeId set_type(ResolvedExpression *expr, TypeId type_id) {
            expr->info = ResolvedTypeInfo{type_id};
            inference_ctx->constrain_context_type(type_id, error::Link(expr->original));
            return type_id;
        }

        TypeId set_type(ResolvedExpression *expr, TypeSymbol symbol) {
            auto type_id = ttable->get_type_id(symbol);
            return set_type(expr, type_id);
        }

        TypeId operator()(ExprTransformStep<ast::IdExpression> step) {
            auto var_info = std::get_if<ResolvedVariableInfo>(&step.original_info());
            if (!var_info) {
                inference_ctx->constrain_context_type(ttable->get_type_id(ErrorTypeSymbol{}),
                                       error::Link{step.original->original, "unknown identifier"});
            }

            auto variable = variables->resolve_variable(var_info->variable_id);
            inference_ctx->constrain_context_type(var_info->variable_id);
            return set_resolved_info(step.out_info());
        }

        TypeId operator()(ExprTransformStep<ast::LiteralExpression> step) {
            // For now we treat all literals as ints...
            return set_type(step.out, NamedTypeSymbol{"int"});
        }

        template <typename Ast>
        InferenceContext make_child_context(ExprTransformStep<Ast> step, size_t child_index) {
            auto node = step.original->children[child_index].original;
            return InferenceContext(variables, ttable, node, errors);
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

        TypeId operator()(ExprTransformStep<ast::BinaryExpression> step) {
            auto left_inference_ctx = make_child_context(step, 0);
            auto left_type_id = resolve_child(step, 0, &left_inference_ctx);

            auto right_inference_ctx = make_child_context(step, 1);
            auto right_type_id = resolve_child(step, 1, &right_inference_ctx);

            // We do not support many implicit conversions. For now, the types must match exactly,
            // or we produce an error type.
            if (!types_equal(left_type_id, right_type_id)) {
                auto type_left = ttable->get_type(left_type_id).get_name();
                auto type_right = ttable->get_type(right_type_id).get_name();
                auto link_left = error::Link{step.ast->get_left(), std::string(type_left)};
                auto link_right = error::Link{step.ast->get_right(), std::string(type_right)};
                errors->report(step.ast,
                               "Expected both binary operands to have matching types, but got ",
                               link_left,
                               " and ",
                               link_right);
            }

            // Assume the output type is the input type (not the case for `==`)
            switch (step.ast->get_operator()) {
            case ast::TokenType::EQUAL_EQUAL:
            case ast::TokenType::NOT_EQUAL:
            case ast::TokenType::LESS:
            case ast::TokenType::LESS_EQUAL:
            case ast::TokenType::GREATER:
            case ast::TokenType::GREATER_EQUAL:
                return set_type(step.out, NamedTypeSymbol{"bool"});
            default:
                return set_type(step.out, left_type_id);
            }
        }

        TypeId operator()(ExprTransformStep<ast::CallExpression> step) {
            auto callee = step.original->children[0];
            auto finfo = std::get_if<ResolvedFunctionInfo>(&callee.info);
            if (!finfo) {
                // For now, the callee must be a function. Soon we'll add function types.
                errors->report(callee.original, "Expected a function in call expression");
                return set_type(step.out, ErrorTypeSymbol{});
            }

            auto func = ftable->get_function(finfo->function_id);
            if (!func) {
                errors->report(callee.original, "Unknown function in call expression");
                return set_type(step.out, ErrorTypeSymbol{});
            }

            auto params = func->get_param_types();
            auto return_type = func->get_return_type();

            if (params->size() != step.original->num_children - 1) {
                add_error_expected(params->size(),
                                   step.original->num_children - 1,
                                   step.ast,
                                   "Argument count mismatch in call expression");
                return set_type(step.out, ErrorTypeSymbol{});
            }

            for (size_t i = 1; i < step.original->num_children; ++i) {
                auto arg_inference_ctx = make_child_context(step, i);
                arg_inference_ctx.constrain_context_type(params->at(i - 1),
                                                         error::Link(step.ast,
                                                                     "argument " +
                                                                         std::to_string(i)));
                auto arg_id = resolve_child(step, i, &arg_inference_ctx);
            }

            if (return_type) {
                inference_ctx->constrain_context_type(return_type.value(),
                                                      error::Link(step.ast, "return type"));
            } else {
                inference_ctx->constrain_context_type(ttable->get_type_id(VoidTypeSymbol{}),
                                                      error::Link(step.ast, "return type"));
            }

            return set_resolved_info(step.out_info());
        }

        TypeId operator()(ExprTransformStep<ast::DotOperatorExpression> step) {
            auto inference_ctx_parent = inference_ctx;
            auto operand_inference_ctx = make_child_context(step, 0);

            switch (step.ast->get_operator()) {
            case ast::TokenType::STAR: {
                operand_inference_ctx.constrain_points_to(inference_ctx_parent, step.ast);
                resolve_child(step, 0, &operand_inference_ctx);
                inference_ctx_parent->constrain_dereferences(&operand_inference_ctx, step.ast);
                break;
            }

            case ast::TokenType::AMP: {
                operand_inference_ctx.constrain_dereferences(inference_ctx_parent, step.ast);
                resolve_child(step, 0, &operand_inference_ctx);
                inference_ctx_parent->constrain_points_to(&operand_inference_ctx, step.ast);
                break;
            }

            default:
                throw std::runtime_error("Unexpected .? operator type: " +
                                         std::string(step.ast->get_operator_token()->text));
            }

            return set_resolved_info(step.out_info());
        }

        TypeId operator()(ExprTransformStep<ast::CastExpression> step) {
            auto casted_type = ttable->get_type(step.ast->get_type()).get_id();
            auto uncast_inference_ctx = make_child_context(step, 0);
            inference_ctx->constrain_context_type(casted_type,
                                                  error::Link(step.ast, "result of cast"));
            auto operand_type = resolve_child(step, 0, &uncast_inference_ctx);
            // TODO: check that cast is valid (e.g., cannot cast bool to struct)

            return set_resolved_info(step.out_info());
        }

        TypeId operator()(ExprTransformStep<ast::MemberAccessExpression> step) {
            // For now we do not support member access, so this is always an error.
            errors->report(step.ast, "Member access is not supported yet");
            return set_type(step.out, ErrorTypeSymbol{});
        }

        TypeId operator()(ExprTransformStep<ast::UnaryPrefixExpression> step) {
            if (step.ast->get_operator() != ast::TokenType::NOT) {
                throw std::runtime_error("Unexpected unary prefix operator: " +
                                         std::string(step.ast->get_operator_token()->text));
            }

            auto bool_id = ttable->get_type_id(NamedTypeSymbol{"bool"});
            auto operand_inference_ctx = make_child_context(step, 0);
            operand_inference_ctx.constrain_context_type(bool_id,
                                                         error::Link(step.ast, "operand of '!'"));
            inference_ctx->constrain_context_type(bool_id, error::Link(step.ast, "result of '!'"));
            resolve_child(step, 0, &operand_inference_ctx);
            return set_resolved_info(step.out_info());
        }

    private:
        const FunctionTable *ftable;
        const TypeTable *ttable;
        ExprTransformMiddleware middleware;
        const VariableRegistry *variables;
        error::Reporter *errors;
        InferenceContext *inference_ctx = nullptr;
    };

    class StatementTypeCheckTransform {
    public:
        StatementTypeCheckTransform(util::Arena *arena,
                                    const VariableRegistry *registry,
                                    const TypeTable *ttable,
                                    TypeCheckTransform *expr_typecheck,
                                    error::Reporter *errors)
            : middleware(arena), variables(registry), ttable(ttable),
              expr_typecheck(expr_typecheck), errors(errors) {}

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

            InferenceContext inference_ctx(variables,
                                           ttable,
                                           step.original->initializer->original,
                                           errors);

            auto variable = variables->resolve_variable(step.out->variable_id);
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
        const VariableRegistry *variables;
        const TypeTable *ttable;
        error::Reporter *errors;
    };

} // namespace

ResolvedExpressionsResult TypeChecker::type_check(
    const std::vector<const ResolvedDeclaration *> &decls, const VariableRegistry *variables) {
    util::Arena arena;
    std::vector<ResolvedDeclaration *> resolved_decls;
    error::Reporter errors;

    TypeCheckTransform typecheck(&arena, ftable, ttable, variables, &errors);
    StatementTypeCheckTransform stmt_typecheck(&arena, variables, ttable, &typecheck, &errors);

    for (auto decl : decls) {
        if (!decl->resolved_stmt) {
            continue;
        }

        auto result = stmt_typecheck.typecheck(*decl->resolved_stmt);
        auto resolved = arena.alloc<ResolvedDeclaration>();
        resolved->original = decl->original;
        resolved->resolved_stmt = result;
        resolved_decls.push_back(resolved);
    }

    return ResolvedExpressionsResult(std::move(arena),
                                     std::move(resolved_decls),
                                     errors.get_errors(),
                                     *variables // intentionally copied.
    );
}