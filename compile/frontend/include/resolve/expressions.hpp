#ifndef ARENA_INCLUDE_RESOLVE_EXPRESSIONS_HPP
#define ARENA_INCLUDE_RESOLVE_EXPRESSIONS_HPP

#include "resolve/tree.hpp"
#include "resolve/error.hpp"

namespace arena::sema {

    class ResolvedExpressionsResult {
    public:
        ResolvedExpressionsResult() = default;

        ResolvedExpressionsResult(util::Arena arena,
                                  std::vector<ResolvedDeclaration *> resolved_decls,
                                  std::vector<ResolveError> errors,
                                VariableRegistry resolved_variables)
            : arena(std::move(arena)), resolved_decls(std::move(resolved_decls)),
              errors(std::move(errors)), resolved_variables(std::move(resolved_variables)) {}

        std::vector<const ResolvedDeclaration *> get_resolved_decls() const {
            return std::vector<const ResolvedDeclaration *>(resolved_decls.begin(),
                                                            resolved_decls.end());
        }

        const VariableRegistry *get_resolved_variables() const { return &resolved_variables; }

        const std::vector<ResolveError> &get_errors() const { return errors; }

        bool operator==(const ResolvedExpressionsResult &other) const;

    private:
        util::Arena arena;
        std::vector<ResolveError> errors;
        std::vector<ResolvedDeclaration *> resolved_decls;
        std::vector<ResolvedStatement *> resolved_stmts;
        VariableRegistry resolved_variables;
    };

    class ExpressionResolver {
    public:
        ResolvedExpressionsResult resolve(const std::vector<ast::Declaration *> &decls,
                                          const FunctionSymbolSet *functions,
                                          const TypeSymbolSet *types,
                                          const TypeSymbolRegistry *registry);
    };
} // namespace arena::sema

#endif