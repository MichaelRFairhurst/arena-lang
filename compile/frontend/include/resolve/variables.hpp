#ifndef ARENA_INCLUDE_RESOLVE_VARIABLES_HPP
#define ARENA_INCLUDE_RESOLVE_VARIABLES_HPP

#include <string_view>
#include "ast/statements.hpp"
#include "signatures/types.hpp"

namespace arena::sema {

    struct VariableId {
        size_t v_id;

        bool operator==(const VariableId &other) const { return v_id == other.v_id; }

        bool operator!=(const VariableId &other) const { return !(*this == other); }
    };

    struct ResolvedVariable {
        std::string_view name;
        const ast::Node *declaration;

        std::optional<TypeId> explicit_type_id;
        std::optional<TypeId> inferred_type_id;

        std::optional<TypeId> get_type_id() const {
            if (explicit_type_id.has_value()) {
                return explicit_type_id;
            } else if (inferred_type_id.has_value()) {
                return inferred_type_id;
            }
            return std::nullopt;
        }

        bool has_type() const {
            return explicit_type_id.has_value() || inferred_type_id.has_value();
        }
    };

    class VariableRegistry {
    public:
        VariableRegistry() = default;

        bool operator==(const VariableRegistry &other) const {
            if (variables.size() != other.variables.size()) {
                return false;
            }

            for (size_t i = 0; i < variables.size(); i++) {
                if (variables[i] != other.variables[i]) {
                    return false;
                }
            }

            return true;
        }

        ResolvedVariable *resolve_variable(VariableId id) const {
            if (id.v_id < variables.size()) {
                return variables[id.v_id];
            }
            throw std::runtime_error("Invalid variable ID: " + std::to_string(id.v_id));
        }

        VariableId add_variable(std::string_view name, ResolvedVariable *var) {
            auto id = VariableId{variables.size()};
            variables.push_back(var);
            return id;
        }

    private:
        std::vector<ResolvedVariable *> variables;
    };

    class VariableScope {
    public:
        VariableScope(VariableRegistry *variable_registry) : variable_registry(variable_registry) {}
        VariableScope(VariableScope *parent)
            : parent(parent), variable_registry(parent->variable_registry) {}

        VariableId add_variable(std::string_view name, ResolvedVariable *variable) {
            auto id = variable_registry->add_variable(name, variable);
            variables[name] = id;
            return id;
        }

        std::optional<VariableId> resolve_variable_id(std::string_view name) const {
            auto it = variables.find(name);
            if (it != variables.end()) {
                return it->second;
            }

            if (parent) {
                return parent->resolve_variable_id(name);
            }

            return std::nullopt;
        }

        std::optional<ResolvedVariable *> resolve_variable(std::string_view name) const {
            auto id = resolve_variable_id(name);
            if (id) {
                return variable_registry->resolve_variable(*id);
            }
            return std::nullopt;
        }

    private:
        std::unordered_map<std::string_view, VariableId> variables;
        VariableRegistry *variable_registry = nullptr;
        VariableScope *parent = nullptr;
    };

} // namespace arena::sema

template <>
struct std::hash<arena::sema::VariableId> {
    std::size_t operator()(const arena::sema::VariableId &id) const {
        return std::hash<size_t>{}(id.v_id);
    }
};

#endif