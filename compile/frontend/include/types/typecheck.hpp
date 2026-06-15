#ifndef ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP
#define ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP

#include "resolve/tree.hpp"
#include "resolve/tree_transform.hpp"
#include "resolve/expressions.hpp"

namespace arena::sema {

    class TypeChecker {
    public:
        TypeChecker(const FunctionTable &ftable, const TypeTable &ttable)
            : ftable(&ftable), ttable(&ttable) {}

        ResolvedExpressionsResult type_check(const std::vector<const ResolvedDeclaration *> &decls,
                                             const VariableRegistry *registry);

    private:
        const FunctionTable *ftable;
        const TypeTable *ttable;
    };

    class TypecheckOperations {
    public:
        TypecheckOperations(const FunctionTable *ftable,
                            const TypeTable *ttable,
                            const VariableRegistry *variables,
                            LifetimeGroup *lifetimes,
                            error::Reporter *errors)
            : ftable(ftable), ttable(ttable), variables(variables), lifetimes(lifetimes),
              errors(errors) {}

        ResolvedType get_type(TypeId id) const;

        std::string get_type_name(TypeId id) const;
        std::string get_type_name(TypeId id, const LifetimeGroup &type_lifetimes) const;

        TypeId substitute_lifetimes(
            TypeId type_id,
            const LifetimeGroup &type_lifetimes,
            const std::unordered_map<LifetimeId, LifetimeId> &substitutions) const;

        void add_error_expected(TypeId expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message);

        void add_error_expected(std::string_view expected,
                                TypeId actual,
                                const ast::Node *node,
                                std::string message);

        void add_error_expected(size_t num_expected,
                                size_t num_actual,
                                const ast::Node *node,
                                std::string message);

        error::Error* require_assignable(TypeId lhs,
                                                       TypeId rhs,
                                                       const ast::Node *node,
                                                       std::string message,
                                                       bool force_strict = false);

        void require_type(TypeId expected,
                          TypeId actual,
                          const ast::Node *node,
                          std::string message);

        bool types_equal(TypeId a, TypeId b);

        const FunctionTable &get_functions() const { return *ftable; }

        const TypeTable &get_types() const { return *ttable; }

        const VariableRegistry &get_variables() const { return *variables; }

        LifetimeGroup &get_lifetimes() { return *lifetimes; }

        error::Reporter &get_errors() { return *errors; }

    private:
        const FunctionTable *ftable;
        const VariableRegistry *variables;
        const TypeTable *ttable;
        LifetimeGroup *lifetimes;
        error::Reporter *errors;
    };

} // namespace arena::sema


#endif