#ifndef ARENA_INCLUDE_RESOLVE_TREE_HPP
#define ARENA_INCLUDE_RESOLVE_TREE_HPP

#include <variant>
#include <stack>
#include "ast/declarations.hpp"
#include "resolve/functions.hpp"
#include "resolve/variables.hpp"
#include "util/arena.hpp"
#include "util/concat.hpp"

namespace arena::sema {

    struct ResolvedFunctionInfo {
        FunctionId function_id;
    };

    struct ResolvedTypeInfo {
        TypeId type_id;
    };

    struct ResolvedVariableInfo {
        VariableId variable_id;
    };

    struct UnresolvedExprInfo {};

    using ResolvedExpressionInfo = std::
        variant<UnresolvedExprInfo, ResolvedFunctionInfo, ResolvedVariableInfo, ResolvedTypeInfo>;

    struct ResolvedExpression;

    struct ResolvedStatement;

    struct ResolvedBlockStatement {
        const ast::BlockStatement *original = nullptr;
        ResolvedStatement *statements = nullptr;
        size_t num_statements = 0;
    };

    struct ResolvedLetStatement {
        const ast::LetStatement *original = nullptr;
        VariableId variable_id;
        ResolvedExpression *initializer = nullptr;
    };

    struct ResolvedIfStatement {
        const ast::IfStatement *original = nullptr;
        ResolvedExpression *condition = nullptr;
        ResolvedStatement *then_branch = nullptr;
        ResolvedStatement *else_branch = nullptr;
    };

    struct ResolvedReturnStatement {
        const ast::ReturnStatement *original = nullptr;
        ResolvedExpression *expr = nullptr;
    };

    struct ResolvedExprStatement {
        const ast::ExpressionStatement *original = nullptr;
        ResolvedExpression *expr = nullptr;
    };

    using ResolvedStatementType = std::variant<ResolvedIfStatement,
                                               ResolvedLetStatement,
                                               ResolvedReturnStatement,
                                               ResolvedBlockStatement,
                                               ResolvedExprStatement>;
    struct ResolvedStatement {
        ResolvedStatementType info;
    };

    struct ResolvedDeclaration {
        const ast::Declaration *original = nullptr;
        ResolvedStatement *resolved_stmt = nullptr;
    };

    struct ResolvedExpression {
        const ast::Expression *original = nullptr;
        ResolvedExpressionInfo info = UnresolvedExprInfo{};
        ResolvedExpression *parent = nullptr;
        ResolvedExpression *children = nullptr;
        size_t num_children = 0;

        std::string to_string() const {
            if (num_children == 0) {
                return original->to_string();
            }

            std::string result = std::string("{") + std::string(original->begin()->text) + "..." +
                                 std::string(original->end()->text) + ":[";
            result += util::concat(children, children + num_children, ", ");
            result += "]}";
            return result;
        }
    };

    class ResolvedExpressionBuilder : public ast::Visitor {
    public:
        ResolvedExpressionBuilder(util::Arena &arena) : arena(&arena) {}

        void set_children(size_t num_children) {
            expr_stack.top()->children = arena->alloc_array<ResolvedExpression>(num_children);
            expr_stack.top()->num_children = num_children;
        }

        void recurse_expr_at(int index, const ast::Expression *expr) {
            expr_stack.push(&expr_stack.top()->children[index]);
            expr_stack.top()->original = expr;
            expr->accept(this);
            expr_stack.pop();
        }

        template <typename... Args>
        void recurse_exprs(Args &&...args) {
            set_children(sizeof...(args));
            recurse_exprs_impl(0, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void recurse_exprs_impl(size_t offset, const ast::Expression *expr, Args &&...args) {
            recurse_expr_at(offset, expr);
            if constexpr (sizeof...(args) > 0) {
                recurse_exprs_impl(offset + 1, std::forward<Args>(args)...);
            }
        }

        void visit_root(const ast::Expression *expr) {
            expr_stack.push(arena->alloc<ResolvedExpression>());
            expr_stack.top()->original = expr;
            resolved_exprs.push_back(expr_stack.top());
            expr->accept(this);
        }

        void visit(const ast::IdExpression *id_expr) override {
            // Noop
        }

        void visit(const ast::CallExpression *call_expr) override {
            ResolvedExpression *resolved = arena->alloc<ResolvedExpression>();
            resolved->original = call_expr;

            set_children(call_expr->get_args().size() + 1);
            recurse_expr_at(0, call_expr->get_callee());
            auto args = call_expr->get_args();
            for (size_t i = 0; i < args.size(); ++i) {
                recurse_expr_at(i + 1, args[i]);
            }
        }

        void visit(const ast::MemberAccessExpression *member_access) override {
            recurse_exprs(member_access->get_object());
        }

        void visit(const ast::CastExpression *cast_expr) override {
            recurse_exprs(cast_expr->get_expr());
        }

        void visit(const ast::BinaryExpression *binary_expr) override {
            recurse_exprs(binary_expr->get_left(), binary_expr->get_right());
        }

        void visit(const ast::DotOperatorExpression *dot_op_expr) override {
            recurse_exprs(dot_op_expr->get_operand());
        }

        void visit(const ast::UnaryPrefixExpression *prefix_expr) override {
            recurse_exprs(prefix_expr->get_operand());
        }

        std::vector<ResolvedExpression *> &get_resolved_exprs() { return resolved_exprs; }

    private:
        std::stack<ResolvedExpression *> expr_stack;
        util::Arena *arena;
        std::vector<ResolvedExpression *> resolved_exprs;
    };

    class ResolvedStatementBuilder : public ast::Visitor {
    public:
        ResolvedStatementBuilder(util::Arena &arena) : arena(&arena) {}

        template <typename T>
        T *resolve_as(const decltype(std::declval<T>().original) decl) {
            stmt_stack.top()->info = T{};
            T *resolved_decl = &std::get<T>(stmt_stack.top()->info);
            resolved_decl->original = decl;
            return resolved_decl;
        }

        ResolvedExpression *resolve_expression(const ast::Expression *expr) {
            ResolvedExpressionBuilder expr_builder(*arena);
            expr_builder.visit_root(expr);
            auto resolved_exprs = expr_builder.get_resolved_exprs();
            if (!resolved_exprs.empty()) {
                return resolved_exprs[0];
            }
            return nullptr;
        }

        void visit(const ast::FunctionDefinition *func_def) override {
            stmt_stack.push(arena->alloc<ResolvedStatement>());
            func_def->get_body()->accept(this);
        }

        void visit(const ast::IfStatement *if_stmt) override {
            auto *resolved = resolve_as<ResolvedIfStatement>(if_stmt);

            resolved->condition = resolve_expression(if_stmt->get_condition());

            resolved->then_branch = arena->alloc<ResolvedStatement>();
            stmt_stack.push(resolved->then_branch);
            if_stmt->get_then()->accept(this);
            stmt_stack.pop();

            if (if_stmt->get_else()) {
                resolved->else_branch = arena->alloc<ResolvedStatement>();
                stmt_stack.push(resolved->else_branch);
                if_stmt->get_else()->accept(this);
                stmt_stack.pop();
            }
        }

        void visit(const ast::ReturnStatement *return_stmt) override {
            auto *resolved = resolve_as<ResolvedReturnStatement>(return_stmt);
            if (return_stmt->get_expr()) {
                resolved->expr = resolve_expression(return_stmt->get_expr());
            }
        }

        void visit(const ast::ExpressionStatement *expr_stmt) override {
            auto *resolved = resolve_as<ResolvedExprStatement>(expr_stmt);
            resolved->expr = resolve_expression(expr_stmt->get_expr());
        }

        void visit(const ast::BlockStatement *ast_block) override {
            auto *resolved = resolve_as<ResolvedBlockStatement>(ast_block);

            resolved->statements =
                arena->alloc_array<ResolvedStatement>(ast_block->get_statements().size());
            resolved->num_statements = ast_block->get_statements().size();
            auto ast_stmts = ast_block->get_statements();
            for (size_t i = 0; i < ast_stmts.size(); ++i) {
                stmt_stack.push(&resolved->statements[i]);
                ast_stmts[i]->accept(this);
                stmt_stack.pop();
            }
        }

        void visit(const ast::LetStatement *let_stmt) override {
            auto *resolved = resolve_as<ResolvedLetStatement>(let_stmt);

            auto initializer = let_stmt->get_initializer();
            if (initializer != nullptr) {
                resolved->initializer = resolve_expression(initializer);
            }
        }

        ResolvedStatement *get_resolved_stmt(const ast::Statement *stmt) {
            stmt_stack.push(arena->alloc<ResolvedStatement>());
            stmt->accept(this);

            if (!stmt_stack.empty()) {
                return stmt_stack.top();
            }
            return nullptr;
        }

    private:
        std::stack<ResolvedStatement *> stmt_stack;
        util::Arena *arena;
    };

    class ResolvedDeclarationBuilder : public ast::Visitor {
    public:
        ResolvedDeclarationBuilder(util::Arena &arena) : arena(&arena) {}

        void visit(const ast::FunctionDeclaration *func_decl) override {
            result = arena->alloc<ResolvedDeclaration>();
            result->original = func_decl;
            result->resolved_stmt = nullptr;
        }

        void visit(const ast::FunctionDefinition *func_def) override {
            result = arena->alloc<ResolvedDeclaration>();
            result->original = func_def;

            ResolvedStatementBuilder stmt_builder(*arena);
            result->resolved_stmt = stmt_builder.get_resolved_stmt(func_def->get_body());
        }

        ResolvedDeclaration *get_resolved_decl() { return result; }

    private:
        ResolvedDeclaration *result = nullptr;
        util::Arena *arena;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_TREE_HPP