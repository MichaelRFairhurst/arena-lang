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
                              std::vector<ResolveError> *errors)
            : current_expr(expr), functions(functions), types(types),
              variable_scope(variable_scope), errors(errors) {}
        using ast::Visitor::visit;

        void visit(const ast::IdExpression *id_expr) override {
            std::string_view ident = id_expr->get_id();

            std::optional<FunctionId> fid = functions->get_id(FunctionSymbol{ident});

            auto var = variable_scope->resolve_variable(ident);
            if (fid && var) {
                errors->emplace_back("Identifier '" + std::string(ident) +
                                         "' is ambiguous (could refer to both a function and a "
                                         "variable)",
                                     id_expr);
            } else if (var) {
                current_expr->info = ResolvedVariableInfo{*var};
            } else if (fid) {
                current_expr->info = ResolvedFunctionInfo{*fid};
            } else {
                errors->emplace_back("Unresolved identifier: " + std::string(ident), id_expr);
            }
        }

    private:
        ResolvedExpression *current_expr;
        const FunctionSymbolSet *functions;
        const TypeSymbolSet *types;
        VariableScope *variable_scope;
        std::vector<ResolveError> *errors;
    };

    class ExpressionResolverVisitor {
    public:
        ExpressionResolverVisitor(const FunctionSymbolSet *functions,
                                  const TypeSymbolSet *types,
                                  std::vector<ResolveError> &errors,
                                  VariableScope *variable_scope)
            : functions(functions), types(types), errors(&errors), variable_scope(variable_scope) {}

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

            auto type = let_stmt.original->get_type();
            if (type != nullptr) {
                // TODO: handle more complex types here (e.g. pointers, arrays, generics, etc.)
                auto symbol = NamedTypeSymbol{type->to_string()};
                explicit_type_id = types->get_id(symbol);
            }

            auto var = ResolvedVariable{
                let_stmt.variable.name,
                let_stmt.variable.declaration,
                explicit_type_id,
                std::nullopt // inferred type id will be filled in during type checking
            };
            variable_scope->add_variable(let_stmt.variable.name, var);
            let_stmt.variable = var;
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
        std::vector<ResolveError> *errors;
        VariableScope *variable_scope = nullptr;
        const FunctionSymbolSet *functions;
        const TypeSymbolSet *types;
    };

    class FunctionScopeVisitor : public ast::Visitor {
    public:
        explicit FunctionScopeVisitor(const TypeSymbolSet *type_symbols) : type_symbols(type_symbols) {}

        void visit(const ast::FunctionDefinition *func_def) override {
            auto param_list = func_def->get_params()->get_params();
            for (const auto &param : param_list) {

                std::optional<TypeId> param_type;

                if (param->get_type() != nullptr) {
                    // TODO: handle more complex types here (e.g. pointers, arrays, generics, etc.)
                    auto symbol = NamedTypeSymbol{param->get_type()->to_string()};
                    param_type = type_symbols->get_id(symbol);
                }

                variable_scope.add_variable(param->get_name(),
                                            ResolvedVariable{
                                                param->get_name(),
                                                param,
                                                param_type,
                                                std::nullopt // No inferred type for parameters
                                            });
            }
        }

        VariableScope *get_variable_scope() { return &variable_scope; }

    private:
        VariableScope variable_scope;
        const TypeSymbolSet *type_symbols;
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
                                                      const TypeSymbolSet *types) {
    util::Arena arena;

    std::vector<ResolveError> errors;
    std::vector<ResolvedDeclaration *> resolved_decls;
    for (auto decl : decls) {
        ResolvedDeclarationBuilder builder{arena};
        decl->accept(&builder);
        auto tree = builder.get_resolved_decl();

        if (tree) {
            resolved_decls.push_back(tree);

            // Resolve parameters into the scope
            FunctionScopeVisitor func_scope_visitor{types};
            decl->accept(&func_scope_visitor);

            // Resolve expressions in the function body
            ExpressionResolverVisitor expr_resolver{functions,
                                                    types,
                                                    errors,
                                                    func_scope_visitor.get_variable_scope()};
            expr_resolver(*tree->resolved_stmt);
        }
    }
    return ResolvedExpressionsResult{std::move(arena), resolved_decls, std::move(errors)};
}