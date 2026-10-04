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
                errors->E_R_VAR_AMBG(id_expr, ident);
            } else if (var) {
                current_expr->info = ResolvedVariableInfo{*var};
            } else if (fid) {
                current_expr->info = ResolvedFunctionInfo{*fid};
            } else {
                errors->E_R_ID_UNKN(id_expr, ident);
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
                                  const TypeSymbolRegistry *registry,
                                  LifetimeTable *lifetimes)
            : arena(arena), functions(functions), types(types), errors(errors),
              variable_scope(variable_scope), symbolizer(registry, lifetimes),
              lifetimes(lifetimes) {}

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
                std::vector<const ast::NamedType *> type_references;
                explicit_type_id = types->get_id(symbolizer.resolve(type, &type_references))
                                       .value_or(types->get_id(ErrorTypeSymbol{}).value());

                for (auto &type_ref : type_references) {
                    if (!types->is_available(NamedTypeSymbol{type_ref->get_name()})) {
                        errors->E_R_TYPE_UNKN(type_ref,
                                              {let_stmt.original,
                                               "let-statement for variable " + std::string{name}});
                    }
                }
            }

            auto var = arena->alloc<ResolvedVariable>(name,
                                                      let_stmt.original,
                                                      lifetimes->get_stack_lifetime(),
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
            auto resolve_inner = [this, &block_stmt]() {
                for (size_t i = 0; i < block_stmt.num_statements; i++) {
                    (*this)(block_stmt.statements[i]);
                }
            };

            if (!visited_root) {
                visited_root = true;
                block_stmt.block_lifetime = lifetimes->get_stack_lifetime();
                resolve_inner();
                return;
            }

            VariableScope *outer = variable_scope;
            VariableScope inner{variable_scope};
            auto scoped_stack_lifetime = lifetimes->push_stack(block_stmt.original);
            block_stmt.block_lifetime = lifetimes->get_stack_lifetime();
            variable_scope = &inner;

            resolve_inner();
            variable_scope = outer;
        }

        void operator()(ResolvedArenaStatement &arena_stmt) {
            auto scoped_arena_lifetime = lifetimes->push_arena(arena_stmt.original);
            arena_stmt.arena_lifetime = lifetimes->get_arena_lifetime();
            (*this)(*arena_stmt.block);
        }

    private:
        util::Arena *arena;
        error::Reporter *errors;
        VariableScope *variable_scope = nullptr;
        const FunctionSymbolSet *functions;
        const TypeSymbolSet *types;
        const TypeSymbolResolver symbolizer;
        LifetimeTable *lifetimes;
        bool visited_root = false;
    };

    class DeclarationVisitor : public ast::Visitor {
    public:
        explicit DeclarationVisitor(util::Arena *arena,
                                    const TypeSymbolSet *type_symbols,
                                    const TypeSymbolRegistry *registry,
                                    VariableScope *variable_scope,
                                    LifetimeTable *lifetimes,
                                    ResolvedDeclaration *resolved_decl,
                                    error::Reporter *errors)
            : arena(arena), type_symbols(type_symbols), symbolizer(registry, lifetimes),
              variable_scope(variable_scope), lifetimes(lifetimes), resolved_decl(resolved_decl),
              errors(errors) {}

        void visit(const ast::FunctionDefinition *func_def) override {
            auto scoped_stack_lifetime = lifetimes->push_stack(func_def->get_body());
            visit(static_cast<const ast::FunctionDeclaration *>(func_def));
        }

        void visit(const ast::FunctionDeclaration *func_decl) override {
            auto param_list = func_decl->get_params()->get_params();
            auto num_params = param_list.size();

            std::optional<TypeId> return_type_id;
            if (func_decl->get_return_type() != nullptr) {
                std::vector<const ast::NamedType *> type_references;
                return_type_id = type_symbols->get_id(
                    symbolizer.resolve(func_decl->get_return_type(), &type_references));

                for (auto &type_ref : type_references) {
                    if (!type_symbols->is_available(NamedTypeSymbol{type_ref->get_name()})) {
                        errors->E_R_TYPE_UNKN(type_ref,
                                              {func_decl->get_return_type(),
                                               "return type of function " +
                                                   std::string{func_decl->get_name()}});
                    }
                }
            } else {
                return_type_id = type_symbols->get_id(VoidTypeSymbol{});
            }

            resolved_decl->info = ResolvedFunctionDeclaration{
                .num_parameters = num_params,
                .parameters = arena->alloc_array<VariableId>(num_params),
                .return_type = return_type_id,
            };

            auto p_resolved_param =
                std::get<ResolvedFunctionDeclaration>(resolved_decl->info).parameters;

            for (const auto &param : param_list) {
                std::optional<TypeId> param_type;
                std::vector<const ast::NamedType *> type_references;

                if (param->get_type() != nullptr) {
                    auto symbol = symbolizer.resolve(param->get_type(), &type_references);
                    param_type = type_symbols->get_id(symbol).value_or(
                        type_symbols->get_id(ErrorTypeSymbol{}).value());
                }

                auto variable = arena->alloc<ResolvedVariable>( // force line break
                    param->get_name(),
                    param,
                    lifetimes->get_stack_lifetime(),
                    param_type,
                    std::nullopt // No inferred type for parameters
                );

                for (auto &type_ref : type_references) {
                    if (!type_symbols->is_available(NamedTypeSymbol{type_ref->get_name()})) {
                        errors->E_R_TYPE_UNKN(type_ref,
                                              {param,
                                               "parameter " + std::string{param->get_name()} +
                                                   " of function " +
                                                   std::string{func_decl->get_name()}});
                    }
                }

                auto variable_id = variable_scope->add_variable(param->get_name(), variable);
                *p_resolved_param = variable_id;
                ++p_resolved_param;
            }
        }

        void visit(const ast::StructDefinition *struct_def) override {
            auto &fields = *struct_def->get_fields();
            auto resolved = ResolvedStructDeclaration{
                .num_members = fields.size(),
            };

            if (fields.empty()) {
                resolved.members = nullptr;
                resolved_decl->info = resolved;
                return;
            }

            resolved.members = arena->alloc<ResolvedStructDeclarationMember>(fields.size());
            auto p_resolved_field = resolved.members;

            for (const auto &field : fields) {
                std::vector<const ast::NamedType *> type_references;
                p_resolved_field->name = field->get_name();
                auto field_type_symbol = symbolizer.resolve(field->get_type(), &type_references);
                p_resolved_field->type_id =
                    type_symbols->get_id(field_type_symbol)
                        .value_or(type_symbols->get_id(ErrorTypeSymbol{}).value());

                for (auto &type_ref : type_references) {
                    if (!type_symbols->is_available(NamedTypeSymbol{type_ref->get_name()})) {
                        errors->E_R_TYPE_UNKN( // force line break
                            type_ref,
                            {
                                field,
                                "declaration of member " + std::string{field->get_name()} +
                                    " in struct " + std::string{struct_def->get_name()},
                            });
                    }
                }

                ++p_resolved_field;
            }
        }

    private:
        util::Arena *arena;
        VariableScope *variable_scope;
        const FunctionTable *my_ftable;
        const TypeSymbolSet *type_symbols;
        TypeSymbolResolver symbolizer;
        LifetimeTable *lifetimes;
        ResolvedDeclaration *resolved_decl;
        error::Reporter *errors;
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
                                                      const FunctionTable *my_ftable,
                                                      const TypeSymbolSet *types,
                                                      const TypeSymbolRegistry *registry) {
    util::Arena arena;

    error::Reporter errors;
    std::vector<ResolvedDeclaration *> resolved_decls;
    VariableRegistry variable_registry;
    for (auto decl : decls) {
        ResolvedDeclarationBuilder builder{arena, *my_ftable};
        decl->accept(&builder);
        auto tree = builder.get_resolved_decl();

        if (!tree) {
            continue;
        }

        VariableScope variable_scope{&variable_registry};

        resolved_decls.push_back(tree);
        // Resolve parameters into the scope
        LifetimeTable public_lifetimes(&tree->lifetimes, true);
        DeclarationVisitor decl_visitor{&arena,
                                        types,
                                        registry,
                                        &variable_scope,
                                        &public_lifetimes,
                                        tree,
                                        &errors};

        decl->accept(&decl_visitor);

        if (tree->resolved_stmt) {
            // Resolve expressions in the function body
            LifetimeTable local_lifetimes(&tree->lifetimes, false);
            ExpressionResolverVisitor expr_resolver{&arena,
                                                    functions,
                                                    types,
                                                    &errors,
                                                    &variable_scope,
                                                    registry,
                                                    &local_lifetimes};

            expr_resolver(*tree->resolved_stmt);
        }
    }
    return ResolvedExpressionsResult{std::move(arena),
                                     resolved_decls,
                                     errors.get_errors(),
                                     std::move(variable_registry)};
}