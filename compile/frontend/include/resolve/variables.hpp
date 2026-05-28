#ifndef ARENA_INCLUDE_RESOLVE_VARIABLES_HPP
#define ARENA_INCLUDE_RESOLVE_VARIABLES_HPP

#include <string_view>
#include "ast/statements.hpp"
#include "resolve/types.hpp"

namespace arena::sema {

    struct ResolvedVariable {
        std::string_view name;
        const ast::LetStatement *declaration;

        std::optional<TypeId> explicit_type_id;
        std::optional<TypeId> inferred_type_id;
    };

    class VariableScope {
        public:
        VariableScope() = default;
        VariableScope(VariableScope *parent) : parent(parent) {}

        void add_variable(std::string_view name, const ast::LetStatement *declaration) {
            variables[name] = ResolvedVariable{name, declaration};
        }

        std::optional<ResolvedVariable> resolve_variable(std::string_view name) const {
            auto it = variables.find(name);
            if (it != variables.end()) {
                return it->second;
            }

            if (parent) {
                return parent->resolve_variable(name);
            }

            return std::nullopt;
        }

        private:
        std::unordered_map<std::string_view, ResolvedVariable> variables;
        VariableScope *parent = nullptr;
    };

} // namespace arena::sema

#endif