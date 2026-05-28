#ifndef ARENA_INCLUDE_RESOLVE_EXPRESSIONS_HPP
#define ARENA_INCLUDE_RESOLVE_EXPRESSIONS_HPP

#include "resolve/tree.hpp"
#include "resolve/error.hpp"

namespace arena::sema {

    class ResolvedExpressionsResult {
    public:
        ResolvedExpressionsResult() = default;

        ResolvedExpressionsResult(util::Arena arena,
                                  std::vector<ResolvedExpression *> resolved_exprs, std::vector<ResolveError> errors)
            : arena(std::move(arena)), resolved_exprs(std::move(resolved_exprs)), errors(std::move(errors)) {}

        std::vector<const ResolvedExpression *> get_resolved_exprs() const {
            return std::vector<const ResolvedExpression *>(resolved_exprs.begin(),
                                                           resolved_exprs.end());
        }

        const std::vector<ResolveError> &get_errors() const {
            return errors;
        }

        bool operator==(const ResolvedExpressionsResult &other) const {
            if (resolved_exprs.size() != other.resolved_exprs.size()) {
                return false;
            }

            if (errors.size() != other.errors.size()) {
                return false;
            }

            for (size_t i = 0; i < resolved_exprs.size(); i++) {
                if (resolved_exprs[i] != other.resolved_exprs[i]) {
                    return false;
                }
            }

            for (size_t i = 0; i < errors.size(); i++) {
                if (errors[i] != other.errors[i]) {
                    return false;
                }
            }

            return true;
        }

    private:
        util::Arena arena;
        std::vector<ResolveError> errors;
        std::vector<ResolvedExpression *> resolved_exprs;
        std::vector<ResolvedStatement *> resolved_stmts;
    };

    class AstIdentifierResolver : public ast::Visitor {
    public:
        AstIdentifierResolver(FunctionTable &ftable,
                              ResolvedExpression *expr,
                              VariableScope *variable_scope,
                              std::vector<ResolveError> *errors)
            : ftable(&ftable), current_expr(expr), variable_scope(variable_scope), errors(errors) {}
        using ast::Visitor::visit;

        void visit(const ast::IdExpression *id_expr) override {
            std::string_view id = id_expr->get_id();
            auto func = ftable->resolve(id);
            auto var = variable_scope->resolve_variable(id);
            if (func && var) {
                errors->emplace_back("Identifier '" + std::string(id) + "' is ambiguous (could refer to both a function and a variable)", id_expr);
            } else if (var) {
                current_expr->info = ResolvedVariableInfo{*var};
            } else if (func) {
                current_expr->info = ResolvedFunctionInfo{func->get_id()};
            } else {
                errors->emplace_back("Unresolved identifier: " + std::string(id), id_expr);
            }
        }

    private:
        FunctionTable *ftable;
        ResolvedExpression *current_expr;
        VariableScope *variable_scope;
        std::vector<ResolveError> *errors;
    };

    class ExpressionResolverVisitor {
    public:
        ExpressionResolverVisitor(FunctionTable &ftable, std::vector<ResolveError> &errors)
            : ftable(&ftable), errors(&errors) {}

        void operator()(ResolvedExpression &expr) {
            // This visitor is only used for visiting the root expression, so we can just resolve it
            // directly
            AstIdentifierResolver identifier_resolver(*ftable, &expr, variable_scope, errors);
            expr.original->accept(&identifier_resolver);

            for (size_t i = 0; i < expr.num_children; i++) {
                (*this)(expr.children[i]);
            }
        }

        void operator()(ResolvedStatement &stmt) { std::visit(*this, stmt.info); }

        void operator()(ResolvedIfStatement &if_stmt) {
            (*this)(*if_stmt.condition);

            VariableScope *outer = variable_scope;

            VariableScope then_scope{outer};
            variable_scope = &then_scope;
            (*this)(*if_stmt.then_branch);


            if (if_stmt.else_branch) {
                VariableScope else_scope{outer};
                variable_scope = &else_scope;
                (*this)(*if_stmt.else_branch);
            }

            variable_scope = outer;
        }

        void operator()(ResolvedLetStatement &let_stmt) {
            variable_scope->add_variable(let_stmt.variable.name, let_stmt.variable.declaration);
        }

        void operator()(ResolvedReturnStatement &return_stmt) { (*this)(*return_stmt.expr); }

        void operator()(ResolvedExprStatement &expr_stmt) { (*this)(*expr_stmt.expr); }

        void operator()(ResolvedBlockStatement &block_stmt) {
            VariableScope *outer = variable_scope;
            VariableScope inner{variable_scope};
            variable_scope = &inner;

            for (size_t i = 0; i < block_stmt.num_statements; i++) {
                (*this)(block_stmt.statements[i]);
            }

            variable_scope = outer;
        }

    private:
        FunctionTable *ftable;
        std::vector<ResolveError> *errors;
        VariableScope *variable_scope = nullptr;
    };

    class ExpressionResolver {
    public:
        ResolvedExpressionsResult resolve(const std::vector<ast::Declaration *> &decls,
                                          FunctionTable &ftable) {
            util::Arena arena;

            std::vector<ResolveError> errors;
            for (auto decl : decls) {
                ResolvedStatementBuilder builder{arena};
                decl->accept(&builder);
                auto tree = builder.get_resolved_stmt();

                if (tree) {
                    ExpressionResolverVisitor expr_resolver{ftable, errors};
                    expr_resolver(*tree);
                }
            }
            return ResolvedExpressionsResult{std::move(arena), {}, std::move(errors)};
        }
    };
} // namespace arena::sema

#endif