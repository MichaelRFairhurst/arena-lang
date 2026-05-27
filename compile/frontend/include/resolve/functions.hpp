#ifndef ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP
#define ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP

#include <vector>
#include <unordered_map>
#include "ast/declarations.hpp"
#include "parse/parse.hpp"
#include "ast/visitor.hpp"
#include "resolve/symbols.hpp"
#include "resolve/types.hpp"

extern "C" {
#include "arena.h"
}


namespace arena::sema {

    class FunctionSymbolRegistry {
    public:
        FunctionSymbolRegistry() {
            rena_arena_init(&registry_arena, RENA_ARENA_LARGE_PAGE_SIZE, 0);
        }

        FunctionSymbolRegistry(const FunctionSymbolRegistry &) = delete;
        FunctionSymbolRegistry(FunctionSymbolRegistry &&) = delete;
        FunctionSymbolRegistry &operator=(const FunctionSymbolRegistry &) = delete;
        FunctionSymbolRegistry &operator=(FunctionSymbolRegistry &&) = delete;

        ~FunctionSymbolRegistry() { rena_arena_free(&registry_arena); }

        FunctionId get_function_id(FunctionSymbol symbol) const {
            return get_function_id(symbol.name);
        }

        FunctionId get_function_id(std::string_view name) const {
            auto it = name_to_id.find(name);
            if (it != name_to_id.end()) {
                return it->second;
            }
            auto id = FunctionId{symbols.size()};
            auto interned_name = intern(name);
            name_to_id[interned_name] = id;
            symbols.push_back(FunctionSymbol{interned_name});
            return id;
        }

        FunctionSymbol get_function_symbol(FunctionId id) const {
            if (id.f_id >= symbols.size()) {
                throw std::runtime_error("Function ID not found");
            }
            return symbols[id.f_id];
        }

        std::string_view intern(std::string_view name) const {
            void *interned;
            rena_arena_alloc(&registry_arena, name.size(), alignof(char), &interned);
            std::memcpy(interned, name.data(), name.size());
            return std::string_view(static_cast<char *>(interned), name.size());
        }

    private:
        mutable std::unordered_map<std::string_view, FunctionId> name_to_id;
        mutable std::vector<FunctionSymbol> symbols;
        mutable rena_arena registry_arena;
    };

    class ResolvedFunction {
    public:
        ResolvedFunction() = default;
        ResolvedFunction(FunctionId id,
                         FunctionSymbol symbol,
                         std::vector<TypeId> param_types,
                         std::optional<TypeId> return_type)
            : id(id), symbol(symbol), param_types(param_types), return_type(return_type) {}

        FunctionId get_id() const { return id; }

    private:
        FunctionId id;
        FunctionSymbol symbol;
        std::vector<TypeId> param_types;
        std::optional<TypeId> return_type;
    };

    class FunctionTable {
    public:
        FunctionTable(const FunctionSymbolRegistry &registry) : registry(&registry) {}

        void add_function(FunctionSymbol symbol,
                          ResolvedFunction function,
                          arena::ast::FunctionDeclaration *source) {
            auto id = registry->get_function_id(symbol);
            functions[id] = function;
            this->source[id] = source;
        }

        std::vector<FunctionId> get_ids() const {
            std::vector<FunctionId> result;
            for (const auto &entry : functions) {
                result.push_back(entry.first);
            }
            return result;
        }

        std::optional<ResolvedFunction> resolve(std::string_view name) const {
            auto id = registry->get_function_id(name);
            auto it = functions.find(id);
            if (it != functions.end()) {
                return it->second;
            }
            return std::nullopt;
        }

    private:
        const FunctionSymbolRegistry *registry;
        std::unordered_map<FunctionId, ResolvedFunction> functions;
        std::unordered_map<FunctionId, arena::ast::FunctionDeclaration *> source;
    };

    class FunctionTableBuilderVisitor : public arena::ast::Visitor {
    public:
        FunctionTableBuilderVisitor(const FunctionSymbolRegistry &registry,
                                    const TypeTable &type_table,
                                    FunctionTable &ftable)
            : ftable(&ftable), type_table(&type_table), registry(&registry) {}

        void visit(arena::ast::FunctionDeclaration *decl) override {
            auto symbol = FunctionSymbol{decl->get_name_token()->text};
            auto id = registry->get_function_id(symbol);
            std::vector<TypeId> param_types;

            auto args = decl->get_args()->get_args();
            for (auto arg : args) {
                auto arg_type = arg->get_type();
                auto arg_type_name = arg_type->to_string();
                auto arg_type_id = type_table->get_type_id(NamedTypeSymbol{arg_type_name});
                param_types.push_back(arg_type_id);
            }

            std::optional<TypeId> return_type_id;
            if (decl->get_return_type()) {
                auto return_type_name = decl->get_return_type()->to_string();
                return_type_id = type_table->get_type_id(NamedTypeSymbol{return_type_name});
            }

            ResolvedFunction function{id, symbol, param_types, return_type_id};
            ftable->add_function(symbol, function, decl);
        }

        void visit(arena::ast::FunctionDefinition *def) override {
            // Function definitions are also function declarations, so we can reuse the same logic.
            visit(static_cast<arena::ast::FunctionDeclaration *>(def));
        }

    private:
        FunctionTable *ftable;
        const TypeTable *type_table;
        const FunctionSymbolRegistry *registry;
    };

    class FunctionTableBuilder {
    public:
        FunctionTableBuilder(const FunctionSymbolRegistry &registry, const TypeTable &type_table)
            : registry(&registry), type_table(&type_table) {}

        FunctionTable build(const std::vector<arena::ast::Declaration *> &declarations) const {
            FunctionTable ftable(*registry);
            FunctionTableBuilderVisitor visitor(*registry, *type_table, ftable);
            for (auto decl : declarations) {
                decl->accept(&visitor);
            }
            return ftable;
        }

    private:
        const FunctionSymbolRegistry *registry;
        const TypeTable *type_table;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP