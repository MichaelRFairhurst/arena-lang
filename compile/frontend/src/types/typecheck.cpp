#include "types/typecheck.hpp"
#include <iostream>

namespace {
    using namespace arena;
    using namespace arena::sema;
    class TypeCheckTransform {
    public:
        TypeCheckTransform(util::Arena *arena,
                           const FunctionTable *ftable,
                           const TypeTable *ttable,
                           std::vector<ResolveError> *errors)
            : middleware(arena), ftable(ftable), ttable(ttable), errors(errors) {}

        ResolvedExpression *typecheck_root(const ResolvedExpression *expr_in) {
            return middleware.transform_root(*expr_in, *this);
        }

        void add_error_expected(TypeId expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message) {
            errors->emplace_back(message + ": expected '" +
                                     std::string(ttable->get_type(expected).get_name()) +
                                     "', got '" + std::string(ttable->get_type(actual).get_name()) +
                                     "'",
                                 node);
        }

        void add_error_expected(std::string_view expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message) {
            errors->emplace_back(message + ": expected " + std::string(expected) + ", got '" +
                                     std::string(ttable->get_type(actual).get_name()) + "'",
                                 node);
        }

        void add_error_expected(size_t num_expected,
                                size_t num_actual,
                                const ast::Node *node,
                                std::string message) {
            errors->emplace_back(message + ": expected '" + std::to_string(num_expected) +
                                     "', got '" + std::to_string(num_actual) + "'",
                                 node);
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

        void add_error_type(TypeId actual, const ast::Node *node, std::string message) {
            errors->emplace_back(message + ", but got '" +
                                     std::string(ttable->get_type(actual).get_name()) + "'",
                                 node);
        }

        void add_error_mismatch(TypeId left,
                                TypeId right,
                                const ast::Node *node,
                                std::string message) {
            errors->emplace_back(message + ", but got types '" +
                                     std::string(ttable->get_type(left).get_name()) + "' and '" +
                                     std::string(ttable->get_type(right).get_name()) + "'",
                                 node);
        }

        void require_types_equal(TypeId a, TypeId b, const ast::Node *node, std::string message) {
            auto left_type = ttable->get_type(a);
            auto right_type = ttable->get_type(b);
            if (left_type.is_error() || right_type.is_error()) {
                // Don't report spurious errors if one of the types is already an error
                return;
            }

            if (a != b) {
                add_error_mismatch(a, b, node, message);
            }
        }

        void add_error_uninferred(std::string_view name, const ast::Node *node) {
            errors->emplace_back("Could not infer type of '" + std::string(name) + "'", node);
        }

        TypeId set_type(ResolvedExpressionInfo &info, TypeId type_id) {
            info = ResolvedTypeInfo{type_id};
            return type_id;
        }

        TypeId set_type(ResolvedExpressionInfo &info, TypeSymbol symbol) {
            auto type_id = ttable->get_type_id(symbol);
            info = ResolvedTypeInfo{type_id};
            return type_id;
        }

        TypeId operator()(ExprTransformStep<ast::IdExpression> step) {
            auto var_info = std::get_if<ResolvedVariableInfo>(&step.original_info());
            if (!var_info) {
                return set_type(step.out_info(), ErrorTypeSymbol{});
            }

            auto inferred = var_info->variable.inferred_type_id;
            auto explicit_ = var_info->variable.explicit_type_id;
            auto type_id =
                explicit_.value_or(inferred.value_or(ttable->get_type_id(ErrorTypeSymbol{})));
            if (!explicit_ && !inferred) {
                add_error_uninferred(var_info->variable.name, step.ast);
            }
            return set_type(step.out_info(), type_id);
        }

        TypeId operator()(ExprTransformStep<ast::LiteralExpression> step) {
            // For now we treat all literals as ints...
            return set_type(step.out_info(), NamedTypeSymbol{"int"});
        }

        TypeId operator()(ExprTransformStep<ast::BinaryExpression> step) {
            auto left_type_id = middleware.transform_child<TypeId>(step, 0, *this);
            auto right_type_id = middleware.transform_child<TypeId>(step, 1, *this);

            // We do not support many implicit conversions. For now, the types must match exactly,
            // or we produce an error type.
            require_types_equal(left_type_id,
                                right_type_id,
                                step.ast,
                                "Expected binary expression operands to have the same type");

            // Assume the output type is the input type (not the case for `==`)
            return set_type(step.out_info(), left_type_id);
        }

        TypeId operator()(ExprTransformStep<ast::CallExpression> step) {
            auto callee = step.original->children[0];
            auto finfo = std::get_if<ResolvedFunctionInfo>(&callee.info);
            if (!finfo) {
                // For now, the callee must be a function. Soon we'll add function types.
                errors->emplace_back("Expected a function in call expression", callee.original);
                return set_type(step.out_info(), ErrorTypeSymbol{});
            }

            auto func = ftable->get_function(finfo->function_id);
            if (!func) {
                errors->emplace_back("Unknown function in call expression", callee.original);
                return set_type(step.out_info(), ErrorTypeSymbol{});
            }

            auto params = func->get_param_types();
            auto return_type = func->get_return_type();

            if (params->size() != step.original->num_children - 1) {
                add_error_expected(params->size(),
                                   step.original->num_children - 1,
                                   step.ast,
                                   "Argument count mismatch in call expression");
                return set_type(step.out_info(), ErrorTypeSymbol{});
            }

            for (size_t i = 1; i < step.original->num_children; ++i) {
                auto arg_id = middleware.transform_child<TypeId>(step, i, *this);
                require_type(params->at(i - 1),
                             arg_id,
                             step.original->children[i].original,
                             "Type mismatch in call expression argument " + std::to_string(i));
            }

            if (return_type.has_value()) {
                return set_type(step.out_info(), return_type.value());
            } else {
                return set_type(step.out_info(), VoidTypeSymbol{});
            }
        }

        TypeId operator()(ExprTransformStep<ast::DotOperatorExpression> step) {
            auto operand_id = middleware.transform_child<TypeId>(step, 0, *this);
            auto operand_symbol = ttable->get_type(operand_id);

            switch (step.ast->get_operator()) {
            case ast::TokenType::STAR: {
                if (auto ptr = std::get_if<PointerType>(&operand_symbol.get_program_type())) {
                    // TODO: handle lifetime
                    return set_type(step.out_info(), ptr->pointee_type);
                } else {
                    if (!std::holds_alternative<ErrorType>(operand_symbol.get_program_type())) {
                        add_error_expected("a pointer type",
                                           operand_id,
                                           step.ast,
                                           "Invalid dereference");
                    }
                    return set_type(step.out_info(), ErrorTypeSymbol{});
                }
            }

            case ast::TokenType::AMP: {
                // TODO: handle lifetime
                TypeSymbol ptr{PointerTypeSymbol{operand_symbol.get_id()}};
                return set_type(step.out_info(), ttable->get_type_id(ptr));
            }


            default:
                throw std::runtime_error("Unexpected .? operator type: " +
                                         std::string(step.ast->get_operator_token()->text));
            }
        }

        TypeId operator()(ExprTransformStep<ast::CastExpression> step) {
            // For now we do not support casts, so this is always an error.
            errors->emplace_back("Casts are not supported yet", step.ast);
            return set_type(step.out_info(), ErrorTypeSymbol{});
        }

        TypeId operator()(ExprTransformStep<ast::MemberAccessExpression> step) {
            // For now we do not support member access, so this is always an error.
            errors->emplace_back("Member access is not supported yet", step.ast);
            return set_type(step.out_info(), ErrorTypeSymbol{});
        }

        TypeId operator()(ExprTransformStep<ast::UnaryPrefixExpression> step) {
            auto operand_type = middleware.transform_child<TypeId>(step, 0, *this);
            if (step.ast->get_operator() != ast::TokenType::NOT) {
                throw std::runtime_error("Unexpected unary prefix operator: " +
                                         std::string(step.ast->get_operator_token()->text));
            }
            // For now we do not support unary operators, so this is always an error.
            auto bool_id = ttable->get_named_type(NamedTypeSymbol{"bool"});
            require_type(bool_id.get_id(), operand_type, step.ast, "Negation of non-bool type");
            return set_type(step.out_info(), ErrorTypeSymbol{});
        }

    private:
        const FunctionTable *ftable;
        const TypeTable *ttable;
        ExprTransformMiddleware middleware;
        std::vector<ResolveError> *errors;
    };

    class StatementTypeCheckTransform {
    public:
        StatementTypeCheckTransform(util::Arena *arena, TypeCheckTransform *expr_typecheck)
            : middleware(arena), expr_typecheck(expr_typecheck) {}

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
            // nothing to do for let statements until we add initializers
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
    };

} // namespace

ResolvedExpressionsResult TypeChecker::type_check(
    const std::vector<const ResolvedDeclaration *> &decls) {
    util::Arena arena;
    std::vector<ResolvedDeclaration *> resolved_decls;
    std::vector<ResolveError> errors;

    TypeCheckTransform typecheck(&arena, ftable, ttable, &errors);
    StatementTypeCheckTransform stmt_typecheck(&arena, &typecheck);

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
                                     std::move(errors));
}