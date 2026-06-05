#include "resolve/expressions.hpp"
#include <iostream>

using namespace arena::sema;
using namespace arena;

namespace {
    class AstIdentifierResolver : public ast::Visitor {
    public:
        AstIdentifierResolver(ResolvedExpression *expr,
                              const FunctionSymbolSet *functions,
                              const TypeSymbolSet *types,
                              VariableScope *variable_scope,
                              error::Reporter *errors)
            : current_expr(expr), functions(functions), types(types),
              variable_scope(variable_scope), errors(errors) {}
        using ast::Visitor::visit;

        void visit(const ast::IdExpression *id_expr) override {
            std::string_view ident = id_expr->get_id();

            std::optional<FunctionId> fid = functions->get_id(FunctionSymbol{ident});

            auto var = variable_scope->resolve_variable_id(ident);
            if (fid && var) {
                errors->report(id_expr, "Identifier '" + std::string(ident) +
                                         "' is ambiguous (could refer to both a function and a "
                                         "variable)");
            } else if (var) {
                current_expr->info = ResolvedVariableInfo{*var};
            } else if (fid) {
                current_expr->info = ResolvedFunctionInfo{*fid};
            } else {
                errors->report(id_expr, "Unresolved identifier: " + std::string(ident));
            }
        }

    private:
        ResolvedExpression *current_expr;
        const FunctionSymbolSet *functions;
        const TypeSymbolSet *types;
        VariableScope *variable_scope;
        error::Reporter *errors;
    };

    class ExpressionResolverVisitor {
    public:
        ExpressionResolverVisitor(util::Arena *arena,
                                  const FunctionSymbolSet *functions,
                                  const TypeSymbolSet *types,
                                  error::Reporter *errors,
                                  VariableScope *variable_scope,
                                  const TypeSymbolRegistry *registry)
            : arena(arena), functions(functions), types(types), errors(errors),
              variable_scope(variable_scope), symbolizer(registry) {}

        void operator()(ResolvedExpression &expr) {
            // This visitor is only used for visiting the root expression, so we can just resolve it
            // directly
            AstIdentifierResolver identifier_resolver(&expr,
                                                      functions,
                                                      types,
                                                      variable_scope,
                                                      errors);
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
            std::optional<TypeId> explicit_type_id;

            auto name = let_stmt.original->get_name();
            auto type = let_stmt.original->get_type();
            if (type != nullptr) {
                explicit_type_id = types->get_id(symbolizer.resolve(type));
            }

            auto var = arena->alloc<ResolvedVariable>(name,
                                                      let_stmt.original,
                                                      explicit_type_id,
                                                      std::nullopt // inferred type id will be
                                                                   // filled in during type checking
            );

            auto id = variable_scope->add_variable(name, var);
            let_stmt.variable_id = id;

            // Note: Ordinarily, we would want to resolve the initializer expression in the previous
            // scope to prevent self-referential assignment. However, we handle this differently. In
            // Arena, we allow initializers to refer to the variable's address, and preventing
            // self-referential initializers is the responsibility of variable initialization
            // analysis.
            if (let_stmt.initializer) {
                (*this)(*let_stmt.initializer);
            }
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
        util::Arena *arena;
        error::Reporter *errors;
        VariableScope *variable_scope = nullptr;
        const FunctionSymbolSet *functions;
        const TypeSymbolSet *types;
        const TypeSymbolResolver symbolizer;
    };

    class FunctionScopeVisitor : public ast::Visitor {
    public:
        explicit FunctionScopeVisitor(util::Arena *arena,
                                      const TypeSymbolSet *type_symbols,
                                      const TypeSymbolRegistry *registry,
                                      VariableScope *variable_scope)
            : arena(arena), type_symbols(type_symbols), symbolizer(registry),
              variable_scope(variable_scope) {}

        void visit(const ast::FunctionDefinition *func_def) override {
            auto param_list = func_def->get_params()->get_params();
            for (const auto &param : param_list) {

                std::optional<TypeId> param_type;

                if (param->get_type() != nullptr) {
                    auto symbol = symbolizer.resolve(param->get_type());
                    param_type = type_symbols->get_id(symbol);
                }

                variable_scope
                    ->add_variable(param->get_name(),
                                   arena->alloc<ResolvedVariable>(param->get_name(),
                                                                  param,
                                                                  param_type,
                                                                  std::nullopt // No inferred type
                                                                               // for parameters
                                                                  ));
            }
        }

    private:
        util::Arena *arena;
        VariableScope *variable_scope;
        const TypeSymbolSet *type_symbols;
        TypeSymbolResolver symbolizer;
    };

} // namespace

bool ResolvedExpressionsResult::operator==(const ResolvedExpressionsResult &other) const {
    if (resolved_decls.size() != other.resolved_decls.size()) {
        return false;
    }

    if (errors.size() != other.errors.size()) {
        return false;
    }

    for (size_t i = 0; i < resolved_decls.size(); i++) {
        if (resolved_decls[i] != other.resolved_decls[i]) {
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


ResolvedExpressionsResult ExpressionResolver::resolve(const std::vector<ast::Declaration *> &decls,
                                                      const FunctionSymbolSet *functions,
                                                      const TypeSymbolSet *types,
                                                      const TypeSymbolRegistry *registry) {
    util::Arena arena;

    error::Reporter errors;
    std::vector<ResolvedDeclaration *> resolved_decls;
    VariableRegistry variable_registry;
    for (auto decl : decls) {
        ResolvedDeclarationBuilder builder{arena};
        decl->accept(&builder);
        auto tree = builder.get_resolved_decl();

        if (tree) {
            resolved_decls.push_back(tree);
            VariableScope variable_scope{&variable_registry};

            // Resolve parameters into the scope
            FunctionScopeVisitor func_scope_visitor{&arena, types, registry, &variable_scope};
            decl->accept(&func_scope_visitor);

            // Resolve expressions in the function body
            ExpressionResolverVisitor expr_resolver{&arena,
                                                    functions,
                                                    types,
                                                    &errors,
                                                    &variable_scope,
                                                    registry};
            expr_resolver(*tree->resolved_stmt);
        }
    }
    return ResolvedExpressionsResult{std::move(arena), resolved_decls, errors.get_errors(), std::move(variable_registry)};
}