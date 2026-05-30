#ifndef ARENA_INCLUDE_SIGNATURES_FUNCTIONS_HPP
#define ARENA_INCLUDE_SIGNATURES_FUNCTIONS_HPP

#include "resolve/symbols.hpp"
#include "ast/declarations.hpp"

namespace arena::sema {

    class ResolvedFunction {
    public:
        ResolvedFunction() = default;
        ResolvedFunction(FunctionId id,
                         FunctionSymbol symbol,
                         std::vector<TypeId> param_types,
                         std::optional<TypeId> return_type)
            : id(id), symbol(symbol), param_types(param_types), return_type(return_type) {}

        FunctionId get_id() const { return id; }

        FunctionSymbol get_symbol() const { return symbol; }

        const std::vector<TypeId> *get_param_types() const { return &param_types; }
        std::optional<TypeId> get_return_type() const { return return_type; }

        bool operator==(const ResolvedFunction &other) const {
            if (id != other.id) {
                return false;
            }

            if (return_type != other.return_type) {
                return false;
            }

            if (param_types.size() != other.param_types.size()) {
                return false;
            }

            for (size_t i = 0; i < param_types.size(); i++) {
                if (param_types[i] != other.param_types[i]) {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const ResolvedFunction &other) const { return !(*this == other); }

    private:
        FunctionId id;
        FunctionSymbol symbol;
        std::vector<TypeId> param_types;
        std::optional<TypeId> return_type;
    };

    class FunctionTable {
    public:
        FunctionTable() = default;
        FunctionTable(const FunctionSymbolRegistry &registry) : registry(&registry) {}

        void add_function(FunctionSymbol symbol,
                          ResolvedFunction function,
                          const arena::ast::FunctionDeclaration *source);

        void import(const FunctionTable &other);

        std::vector<FunctionId> get_ids() const;

        std::vector<const ResolvedFunction *> get_functions() const;

        std::optional<ResolvedFunction> resolve(std::string_view name) const;

        std::optional<ResolvedFunction> get_function(FunctionId id) const;

        bool operator==(const FunctionTable &other) const {
            if (functions.size() != other.functions.size() ||
                source.size() != other.source.size()) {
                return false;
            }
            std::sort(function_list.begin(),
                      function_list.end(),
                      [](const ResolvedFunction &a, const ResolvedFunction &b) {
                          return a.get_id().f_id < b.get_id().f_id;
                      });
            std::sort(other.function_list.begin(),
                      other.function_list.end(),
                      [](const ResolvedFunction &a, const ResolvedFunction &b) {
                          return a.get_id().f_id < b.get_id().f_id;
                      });

            for (size_t i = 0; i < function_list.size(); i++) {
                auto ours = function_list[i];
                auto theirs = other.function_list[i];
                if (ours != theirs) {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const FunctionTable &other) const { return !(*this == other); }

    private:
        const FunctionSymbolRegistry *registry = nullptr;
        std::unordered_map<FunctionId, ResolvedFunction> functions;
        std::unordered_map<FunctionId, const arena::ast::FunctionDeclaration *> source;
        mutable std::vector<ResolvedFunction> function_list; // For equality checking
    };

} // namespace arena::sema

#endif // ARENA_INCLUDE_SIGNATURES_FUNCTIONS_HPP