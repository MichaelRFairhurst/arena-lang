#ifndef ARENA_FRONTEND_INCLUDE_TYPES_OPERATIONS_HPP
#define ARENA_FRONTEND_INCLUDE_TYPES_OPERATIONS_HPP

#include "signatures/functions.hpp"
#include "signatures/types.hpp"
#include "resolve/variables.hpp"
#include "signatures/lifetimes.hpp"

namespace arena::sema {
    class TypeOperations {
    public:
        TypeOperations(const FunctionTable *ftable,
                       const TypeTable *ttable,
                       const VariableRegistry *variables,
                       LifetimeGroup *lifetimes,
                       error::Reporter *errors)
            : ftable(ftable), ttable(ttable), variables(variables), lifetimes(lifetimes),
              errors(errors) {}

        ResolvedType get_type(TypeId id) const;

        std::string get_type_name(TypeId id) const;
        std::string get_type_name(TypeId id, const LifetimeGroup &type_lifetimes) const;

        ResolvedType get_error_type() const;

        std::optional<ResolvedType> dereference(TypeId id) const;
        std::optional<LifetimeId> pointed_lifetime(TypeId id) const;

        bool is_const_strict(TypeId id) const;
        bool is_lifetime_strict(TypeId id) const;

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

        struct assignment_context {
            bool is_copy = true;
            bool is_const = false;
        };

        error::Error *require_assignable(TypeId lhs,
                                         TypeId rhs,
                                         const ast::Node *node,
                                         std::string message,
                                         assignment_context context = {.is_copy = true,
                                                                       .is_const = false});

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

#endif // ARENA_FRONTEND_INCLUDE_TYPES_OPERATIONS_HPP