#ifndef ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP
#define ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP

#include <string_view>

namespace arena::sema {
    struct FunctionId {
        FunctionId() = default;

        size_t f_id = -1;
        
        bool operator==(const FunctionId &other) const {
            return f_id == other.f_id;
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

    using TypeSymbol = std::variant<NamedTypeSymbol, ArrayTypeSymbol, PointerTypeSymbol>;

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

#endif // ARENA_INCLUDE_RESOLVE_SYMBOLS_HPP
