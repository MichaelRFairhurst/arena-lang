#ifndef ARENA_INCLUDE_RESOLVE_TREE_TRANSFORM_HPP
#define ARENA_INCLUDE_RESOLVE_TREE_TRANSFORM_HPP

#include "resolve/tree.hpp"

namespace arena::sema {
    template <typename AstNode>
    struct ExprTransformStep {
        const AstNode *ast;
        const ResolvedExpression *original;
        ResolvedExpression *out;

        const ResolvedExpressionInfo &original_info() const { return original->info; }
        ResolvedExpressionInfo &out_info() { return out->info; }
    };

    class ExprTransformMiddleware {
    public:
        ExprTransformMiddleware(util::Arena *arena) : arena(arena) {}

        template <typename Func, typename Return>
        struct ExprVisitor : public ast::Visitor {
            using ResultType =
                std::conditional_t<std::is_same_v<Return, void>, std::nullptr_t, Return>;

            ExprVisitor(Func &transform_func, const ResolvedExpression *in, ResolvedExpression *out)
                : transform_func(transform_func), in(in), out(out), result() {}

            Func &transform_func;
            const ResolvedExpression *in;
            ResolvedExpression *out;
            ResultType result;

            template <typename Ast>
            void handle(const Ast *ast) {
                auto step = ExprTransformStep<Ast>{.ast = ast, .original = in, .out = out};
                if constexpr (std::is_same_v<Return, void>) {
                    transform_func(step);
                } else {
                    result = transform_func(step);
                }
            }

            void visit(const ast::IdExpression *id_expr) { handle<ast::IdExpression>(id_expr); }
            void visit(const ast::LiteralExpression *lit_expr) {
                handle<ast::LiteralExpression>(lit_expr);
            }
            void visit(const ast::BinaryExpression *bin_expr) {
                handle<ast::BinaryExpression>(bin_expr);
            }
            void visit(const ast::UnaryPrefixExpression *un_expr) {
                handle<ast::UnaryPrefixExpression>(un_expr);
            }
            void visit(const ast::DotOperatorExpression *dot_expr) {
                handle<ast::DotOperatorExpression>(dot_expr);
            }
            void visit(const ast::MemberAccessExpression *member_expr) {
                handle<ast::MemberAccessExpression>(member_expr);
            }
            void visit(const ast::CallExpression *call_expr) {
                handle<ast::CallExpression>(call_expr);
            }
            void visit(const ast::CastExpression *cast_expr) {
                handle<ast::CastExpression>(cast_expr);
            }
        };

        template <typename Func>
        ResolvedExpression *transform_root(const ResolvedExpression &expr_in,
                                           Func &transform_func) {
            auto expr_out = arena->alloc<ResolvedExpression>();
            expr_out->original = expr_in.original;
            expr_out->num_children = expr_in.num_children;
            expr_out->children = arena->alloc_array<ResolvedExpression>(expr_in.num_children);

            auto visitor = ExprVisitor<Func, void>(transform_func, &expr_in, expr_out);
            expr_in.original->accept(&visitor);
            return expr_out;
        }

        template <typename Return = void, typename Ast, typename Func>
        Return transform_child(ExprTransformStep<Ast> parent_info,
                               size_t child_index,
                               Func &transform_func) {
            auto child_in = &parent_info.original->children[child_index];
            auto child_out = &parent_info.out->children[child_index];

            child_out->original = child_in->original;
            child_out->num_children = child_in->num_children;
            child_out->children = arena->alloc_array<ResolvedExpression>(child_in->num_children);

            auto visitor = ExprVisitor<Func, Return>(transform_func, child_in, child_out);
            child_in->original->accept(&visitor);
            if constexpr (!std::is_same_v<Return, void>) {
                return visitor.result;
            }
        }


    private:
        util::Arena *arena;
    };

    class CopyResolvedExpression {
        CopyResolvedExpression(util::Arena *arena) : middleware(arena) {}

    public:
        ResolvedExpression *copy(const ResolvedExpression &expr_in) {
            return middleware.transform_root(expr_in, *this);
        }

        template <typename Ast>
        ResolvedExpression *operator()(ExprTransformStep<Ast> step) {
            auto expr_in = step.original;
            auto expr_out = step.out;
            step.out_info() = step.original_info();

            // By default, just copy the expression and recursively transform children
            for (size_t i = 0; i < expr_in->num_children; ++i) {
                middleware.transform_child(step, i, *this);
            }

            return expr_out;
        }

    private:
        ExprTransformMiddleware middleware;
    };

    template <typename ResolvedStmt>
    struct StmtTransformStep {
        const ResolvedStmt *original;
        ResolvedStmt *out;
    };

    class StmtTransformMiddleware {
    public:
        StmtTransformMiddleware(util::Arena *arena) : arena(arena) {}

        template <typename Func>
        ResolvedStatement *transform_root(const ResolvedStatement *stmt_in, Func &transform_func) {
            auto stmt_out = arena->alloc<ResolvedStatement>();
            std::visit(
                [this, stmt_out, &stmt_in, &transform_func](auto &&in_info) {
                    using T = std::decay_t<decltype(in_info)>;
                    T &out_info = copy(in_info, stmt_out);
                    transform_func(StmtTransformStep<T>{.original = &in_info, .out = &out_info});
                },
                stmt_in->info);
            return stmt_out;
        }

        template <typename Return = void, typename Func>
        Return transform_pair(StmtTransformStep<ResolvedStatement> pair, Func &transform_func) {
            auto child_in = pair.original;
            auto child_out = pair.out;

            return std::visit(
                [this, &child_out, &transform_func](auto &&in_info) {
                    using T = std::decay_t<decltype(in_info)>;
                    auto &out_info = copy(in_info, child_out);
                    transform_func(StmtTransformStep<T>{.original = &in_info, .out = &out_info});
                },
                child_in->info);
        }

        template <typename Return = void, typename Func>
        Return transform_block_child(StmtTransformStep<ResolvedBlockStatement> block_step,
                                     size_t child_index,
                                     Func &transform_func) {
            ResolvedStatement *child_in = &block_step.original->statements[child_index];
            ResolvedStatement *child_out = &block_step.out->statements[child_index];
            return transform_pair(StmtTransformStep<ResolvedStatement>{.original = child_in,
                                                                       .out = child_out},
                                  transform_func);
        }

        template <typename Return = void, typename Func>
        Return transform_arena_block(StmtTransformStep<ResolvedArenaStatement> arena_step,
                                 Func &transform_func) {
            ResolvedStatement *child_in = arena_step.original->block;
            ResolvedStatement *child_out = arena_step.out->block;
            return transform_pair(StmtTransformStep<
                                      ResolvedStatement>{.original = child_in,
                                                         .out = child_out},
                                  transform_func);
        }

        template <typename Return = void, typename Func>
        Return transform_if_then(StmtTransformStep<ResolvedIfStatement> if_step,
                                 Func &transform_func) {
            return transform_pair(StmtTransformStep<
                                      ResolvedStatement>{.original = if_step.original->then_branch,
                                                         .out = if_step.out->then_branch},
                                  transform_func);
        }

        template <typename Return = void, typename Func>
        std::conditional_t<std::is_void_v<Return>, void, std::optional<Return>> transform_if_else(
            StmtTransformStep<ResolvedIfStatement> if_step, Func &transform_func) {
            if (if_step.original->else_branch) {
                auto step =
                    StmtTransformStep<ResolvedStatement>{.original = if_step.original->else_branch,
                                                         .out = if_step.out->else_branch};

                if constexpr (!std::is_void_v<Return>) {
                    return transform_pair(step, transform_func);
                }
            }

            if constexpr (!std::is_void_v<Return>) {
                return std::nullopt;
            }
        }

        ResolvedIfStatement &copy(const ResolvedIfStatement &if_stmt, ResolvedStatement *out) {
            out->info = ResolvedIfStatement{};
            auto &resolved = std::get<ResolvedIfStatement>(out->info);
            resolved.original = if_stmt.original;

            resolved.condition = arena->alloc<ResolvedExpression>();
            resolved.then_branch = arena->alloc<ResolvedStatement>();
            if (if_stmt.else_branch) {
                resolved.else_branch = arena->alloc<ResolvedStatement>();
            }
            return resolved;
        }

        ResolvedBlockStatement &copy(const ResolvedBlockStatement &block_in,
                                     ResolvedStatement *out) {
            out->info = ResolvedBlockStatement{};
            auto &resolved = std::get<ResolvedBlockStatement>(out->info);

            resolved.num_statements = block_in.num_statements;
            resolved.statements = arena->alloc_array<ResolvedStatement>(block_in.num_statements);
            resolved.block_lifetime = block_in.block_lifetime;
            return resolved;
        }

        ResolvedArenaStatement &copy(const ResolvedArenaStatement &arena_stmt, ResolvedStatement *out) {
            out->info = ResolvedArenaStatement{};
            auto &resolved = std::get<ResolvedArenaStatement>(out->info);
            resolved.original = arena_stmt.original;
            resolved.block = arena->alloc<ResolvedStatement>();
            resolved.arena_lifetime = arena_stmt.arena_lifetime;
            return resolved;
        }

        ResolvedLetStatement &copy(const ResolvedLetStatement &let_stmt, ResolvedStatement *out) {
            out->info = ResolvedLetStatement{};
            auto &resolved = std::get<ResolvedLetStatement>(out->info);
            resolved.original = let_stmt.original;
            return resolved;
        }

        ResolvedReturnStatement &copy(const ResolvedReturnStatement &return_stmt,
                                      ResolvedStatement *out) {
            out->info = ResolvedReturnStatement{};
            auto &resolved = std::get<ResolvedReturnStatement>(out->info);
            resolved.original = return_stmt.original;
            if (return_stmt.expr) {
                resolved.expr = arena->alloc<ResolvedExpression>();
            }
            return resolved;
        }

        ResolvedExprStatement &copy(const ResolvedExprStatement &expr_stmt,
                                    ResolvedStatement *out) {
            out->info = ResolvedExprStatement{};
            auto &resolved = std::get<ResolvedExprStatement>(out->info);
            resolved.original = expr_stmt.original;
            if (expr_stmt.expr) {
                resolved.expr = arena->alloc<ResolvedExpression>();
            }
            return resolved;
        }

    private:
        util::Arena *arena;
    };

    class CopyResolvedStatement {
        CopyResolvedStatement(util::Arena *arena) : middleware(arena) {}

    public:
        ResolvedStatement *copy(const ResolvedStatement &stmt_in) {
            return middleware.transform_root(&stmt_in, *this);
        }

        void operator()(StmtTransformStep<ResolvedBlockStatement> step) {
            // By default, just copy the expression and recursively transform children
            for (size_t i = 0; i < step.original->num_statements; ++i) {
                middleware.transform_block_child(step, i, *this);
            }
        }

        void operator()(StmtTransformStep<ResolvedArenaStatement> step) {
            middleware.transform_arena_block(step, *this);
        }

        void operator()(StmtTransformStep<ResolvedIfStatement> step) {
            middleware.transform_if_then(step, *this);
            middleware.transform_if_else(step, *this);
        }

        void operator()(StmtTransformStep<ResolvedLetStatement> step) {
            // nothing to do for let statements until we add initializers
        }

        void operator()(StmtTransformStep<ResolvedReturnStatement> step) {
            // nothing to do for return statements until we add expression transformations
        }

        void operator()(StmtTransformStep<ResolvedExprStatement> step) {
            // nothing to do for expression statements until we add expression transformations
        }

    private:
        StmtTransformMiddleware middleware;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_TREE_TRANSFORM_HPP