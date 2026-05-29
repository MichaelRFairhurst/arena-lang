#ifndef ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP
#define ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP

#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <optional>

extern "C" {
#include "arena.h"
}

namespace arena::sema {
    struct FunctionId {
        FunctionId() = default;

        size_t f_id = -1;
        
        bool operator==(const FunctionId &other) const {
            return f_id == other.f_id;
        }

        bool operator!=(const FunctionId &other) const {
            return !(*this == other);
        }
    };

    struct FunctionSymbol {
        FunctionSymbol() = default;

        std::string_view name = "";

        bool operator==(const FunctionSymbol &other) const {
            return name == other.name;
        }
    };

    struct TypeId {
        TypeId() = default;

        size_t t_id = -1;

        bool operator==(const TypeId &other) const {
            return t_id == other.t_id;
        }

        bool operator!=(const TypeId &other) const {
            return !(*this == other);
        }
    };

    struct NamedTypeSymbol {
        std::string_view name;

        bool operator==(const NamedTypeSymbol &other) const {
            return name == other.name;
        }
    };

    struct ArrayTypeSymbol {
        TypeId element_type;
        size_t size;
        
        bool operator==(const ArrayTypeSymbol &other) const {
            return element_type.t_id == other.element_type.t_id && size == other.size;
        }
    };

    struct PointerTypeSymbol {
        TypeId pointee_type;
        std::optional<std::string_view> lifetime;

        bool operator==(const PointerTypeSymbol &other) const {
            return pointee_type.t_id == other.pointee_type.t_id && lifetime == other.lifetime;
        }
    };

    struct VoidTypeSymbol {
        bool operator==(const VoidTypeSymbol &) const {
            return true; // All void type symbols are considered equal
        }
    };

    struct ErrorTypeSymbol {
        bool operator==(const ErrorTypeSymbol &) const {
            return true; // All error type symbols are considered equal
        }
    };

    using TypeSymbol = std::variant<NamedTypeSymbol, ArrayTypeSymbol, PointerTypeSymbol, VoidTypeSymbol, ErrorTypeSymbol>;

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
} // namespace arena::sema


template<>
struct std::hash<arena::sema::FunctionSymbol> {
    size_t operator()(const arena::sema::FunctionSymbol &symbol) const {
        return std::hash<std::string_view>()(symbol.name);
    }
};

template<>
struct std::hash<arena::sema::FunctionId> {
    size_t operator()(const arena::sema::FunctionId &id) const {
        return std::hash<size_t>()(id.f_id);
    }
};

template<>
struct std::hash<arena::sema::TypeSymbol> {
    size_t operator()(const arena::sema::TypeSymbol &symbol) const {
        return std::visit(*this, symbol);
    }

    size_t operator()(const arena::sema::NamedTypeSymbol &symbol) const {
        return std::hash<std::string_view>()(symbol.name);
    }

    size_t operator()(const arena::sema::ArrayTypeSymbol &symbol) const {
        return std::hash<size_t>()(symbol.element_type.t_id) ^ std::hash<size_t>()(symbol.size);
    }

    size_t operator()(const arena::sema::PointerTypeSymbol &symbol) const {
        size_t hash = std::hash<size_t>()(symbol.pointee_type.t_id);
        if (symbol.lifetime.has_value()) {
            hash ^= std::hash<std::string_view>()(symbol.lifetime.value());
        }
        return hash;
    }

    size_t operator()(const arena::sema::VoidTypeSymbol &) const {
        return 1; // All void type symbols hash to the same values
    }

    size_t operator()(const arena::sema::ErrorTypeSymbol &) const {
        return 0; // All error type symbols hash to the same value
    }

    template <typename T>
    size_t operator()(T) const {
        static_assert(false && sizeof(T), "Non-exhaustive visitor!");
        return 0;
    }
};

template<>
struct std::hash<arena::sema::TypeId> {
    size_t operator()(const arena::sema::TypeId &id) const {
        return std::hash<size_t>()(id.t_id);
    }
};

namespace arena::sema {

    class TypeSymbolRegistry {
    public:
        TypeSymbolRegistry() { rena_arena_init(&registry_arena, RENA_ARENA_LARGE_PAGE_SIZE, 0); }

        TypeSymbolRegistry(const TypeSymbolRegistry &) = delete;
        TypeSymbolRegistry(TypeSymbolRegistry &&) = delete;
        TypeSymbolRegistry &operator=(const TypeSymbolRegistry &) = delete;
        TypeSymbolRegistry &operator=(TypeSymbolRegistry &&) = delete;

        ~TypeSymbolRegistry() { rena_arena_free(&registry_arena); }

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
            } else if (std::get_if<VoidTypeSymbol>(&symbol)) {
                return VoidTypeSymbol{};
            } else if (std::get_if<ErrorTypeSymbol>(&symbol)) {
                return ErrorTypeSymbol{};
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

    class FunctionTable;

    class FunctionSymbolSet {
        public:
        FunctionSymbolSet() = default;
        explicit FunctionSymbolSet(const FunctionTable *ftable);

        void import(const FunctionSymbolSet &other);

        std::optional<FunctionId> get_id(FunctionSymbol symbol) const;

        bool operator==(const FunctionSymbolSet &other) const;
        bool operator!=(const FunctionSymbolSet &other) const { return !(*this == other); }

        private:
        std::unordered_map<FunctionSymbol, FunctionId> symbol_to_id;
        std::vector<FunctionId> sorted_ids; // For equality checking
    };

    class TypeTable;

    class TypeSymbolSet {
        public:
        TypeSymbolSet() = default;
        explicit TypeSymbolSet(const TypeTable *ttable);

        void import(const TypeSymbolSet &other);

        std::optional<TypeId> get_id(TypeSymbol symbol) const;

        bool operator==(const TypeSymbolSet &other) const;
        bool operator!=(const TypeSymbolSet &other) const { return !(*this == other); }

        private:
        std::unordered_map<TypeSymbol, TypeId> symbol_to_id;
        std::vector<TypeId> sorted_ids; // For equality checking
    };


} // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP