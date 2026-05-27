#ifndef ARENA_INCLUDE_RESOLVE_TYPES_HPP
#define ARENA_INCLUDE_RESOLVE_TYPES_HPP

#include <vector>
#include <unordered_map>
#include <variant>
#include "ast/declarations.hpp"
#include "parse/parse.hpp"
#include "ast/visitor.hpp"
#include "resolve/symbols.hpp"

extern "C" {
#include "arena.h"
}

namespace arena::sema {

    class TypeSymbolRegistry {
        public:
        TypeSymbolRegistry() {
            rena_arena_init(&registry_arena, RENA_ARENA_LARGE_PAGE_SIZE, 0);
        }

        TypeSymbolRegistry(const TypeSymbolRegistry &) = delete;
        TypeSymbolRegistry(TypeSymbolRegistry &&) = delete;
        TypeSymbolRegistry &operator=(const TypeSymbolRegistry &) = delete;
        TypeSymbolRegistry &operator=(TypeSymbolRegistry &&) = delete;

         ~TypeSymbolRegistry() {
            rena_arena_free(&registry_arena);
        }

        TypeId get_type_id(TypeSymbol symbol) const {
            auto it = name_to_id.find(symbol);
            if (it != name_to_id.end()) {
                return it->second;
            }
            auto id = TypeId{types.size()};
            auto interned_symbol = intern(symbol);
            name_to_id[interned_symbol] = id;
            types.push_back(interned_symbol);
            return id;
        }

        TypeSymbol get_type_symbol(TypeId id) const {
            if (types.size() <= id.t_id) {
                throw std::runtime_error("Type ID not found");
            }
            return types[id.t_id];
        }

        TypeSymbol intern(TypeSymbol symbol) const {
            if (NamedTypeSymbol *named = std::get_if<NamedTypeSymbol>(&symbol)) {
                auto interned_name = intern(named->name);
                return NamedTypeSymbol{interned_name};
            } else if (ArrayTypeSymbol *array = std::get_if<ArrayTypeSymbol>(&symbol)) {
                return *array;
            } else if (PointerTypeSymbol *pointer = std::get_if<PointerTypeSymbol>(&symbol)) {
                std::optional<std::string_view> interned_lifetime;
                if (pointer->lifetime.has_value()) {
                    interned_lifetime = intern(pointer->lifetime.value());
                }
                return PointerTypeSymbol{pointer->pointee_type, interned_lifetime};
            } else {
                throw std::runtime_error("Unknown type symbol variant");
            }
        }

        std::string_view intern(std::string_view str) const {
            void *interned;
            rena_arena_alloc(&registry_arena, str.size(), alignof(char), &interned);
            std::memcpy(interned, str.data(), str.size());
            return std::string_view(static_cast<char *>(interned), str.size());
        }

        private:
        mutable std::unordered_map<TypeSymbol, TypeId> name_to_id;
        mutable std::vector<TypeSymbol> types;
        mutable rena_arena registry_arena;
    };

    class ResolvedType {
        public:
        ResolvedType(TypeId id) : id(id) {}

        TypeId get_id() const { return id; }

        private:
        TypeId id;
        int size_bytes;
    };

    class TypeTable {
        static TypeTable builtin_type_table(TypeSymbolRegistry &registry) {
            std::vector<TypeSymbol> builtin_types = {
                NamedTypeSymbol{"char"},
                NamedTypeSymbol{"byte"},
                NamedTypeSymbol{"short"},
                NamedTypeSymbol{"ushort"},
                NamedTypeSymbol{"int"},
                NamedTypeSymbol{"uint"},
                NamedTypeSymbol{"long"},
                NamedTypeSymbol{"ulong"},
                NamedTypeSymbol{"usize"},
                NamedTypeSymbol{"ssize"},
                NamedTypeSymbol{"float32"},
                NamedTypeSymbol{"float64"},
                NamedTypeSymbol{"double32"},
                NamedTypeSymbol{"double64"},
            };

            TypeTable table{registry};
            for (const auto &symbol : builtin_types) {
                table.registry.get_type_id(symbol);
            }
            return table;
        }

        public:
        TypeTable(const TypeSymbolRegistry &registry) : registry(registry) {}

        TypeId get_type_id(TypeSymbol symbol) const {
            return registry.get_type_id(symbol);
        }

        private:
        const TypeSymbolRegistry &registry;
    };
};

#endif // ARENA_INCLUDE_RESOLVE_TYPES_HPP