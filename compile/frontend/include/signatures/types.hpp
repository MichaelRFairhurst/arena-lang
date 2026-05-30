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

    class TypeSymbolResolver {
    public:
        TypeSymbolResolver(const TypeSymbolRegistry *registry) : registry(registry) {}

        TypeSymbol resolve(const ast::Type *type) const;

    private:
        const TypeSymbolRegistry *registry;
    };

    struct IntegralType {
        bool is_signed;
        size_t size_bytes;
        std::string_view name;
    };

    struct FloatingType {
        bool is_double_precision;
        size_t size_bytes;
        std::string_view name;
    };

    struct PointerType {
        TypeId pointee_type;
        std::optional<std::string_view> lifetime;
    };

    struct ArrayType {
        TypeId element_type;
        size_t size;
    };

    struct VoidType {};

    struct ErrorType {};

    using ProgramType = std::variant<IntegralType, FloatingType, PointerType, ArrayType, VoidType, ErrorType>;

    class ResolvedType {
    public:
        ResolvedType(TypeId id, ProgramType program_type, TypeSymbol symbol, std::string_view name)
            : id(id), program_type(program_type), symbol(symbol), name(name) {}

        TypeId get_id() const { return id; }
        
        TypeSymbol get_symbol() const { return symbol; }

        bool is_void() const { return std::holds_alternative<VoidType>(program_type); }

        bool is_error() const { return std::holds_alternative<ErrorType>(program_type); }

        const ProgramType &get_program_type() const { return program_type; }

        std::string_view get_name() const { return name; }

    private:
        std::string_view name;
        TypeId id;
        TypeSymbol symbol;
        ProgramType program_type;
    };

    class TypeTable {
    public:
        static TypeTable builtin_type_table(const TypeSymbolRegistry &registry);

        TypeTable(const TypeSymbolRegistry &registry) : registry(registry) {}

        TypeId get_type_id(TypeSymbol symbol) const { return registry.get_type_id(symbol); }

        std::vector<const ResolvedType*> get_types() const;

        ResolvedType get_type(TypeId id) const;

        ResolvedType get_named_type(NamedTypeSymbol named) const;

    private:
        const TypeSymbolRegistry &registry;
        std::unordered_map<TypeId, ResolvedType> types;
    };


}; // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_TYPES_HPP