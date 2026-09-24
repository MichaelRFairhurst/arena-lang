#ifndef ARENA_INCLUDE_RESOLVE_TYPES_HPP
#define ARENA_INCLUDE_RESOLVE_TYPES_HPP

#include <vector>
#include <unordered_map>
#include <variant>
#include "ast/declarations.hpp"
#include "parse/parse.hpp"
#include "ast/visitor.hpp"
#include "resolve/symbols.hpp"
#include "signatures/lifetimes.hpp"

extern "C" {
#include "arena.h"
}

namespace arena::sema {

    struct StructType {
        std::string_view name;
    };

    enum class IntegralLiteralKind {
        Int,
        Char,
        Bool
    };

    struct IntegralType {
        bool is_signed;
        size_t size_bytes;
        std::string_view name;
        IntegralLiteralKind literal_kind;

        uint64_t max_value() const {
            // Be careful to avoid UB by shifting a 64-bit value by 64 or more bits
            auto max64 = std::numeric_limits<uint64_t>::max();
            auto shift_amount = (8 - size_bytes) * 8 + (is_signed ? 1 : 0);
            return max64 >> shift_amount;
        }

        uint64_t min_value_abs() const {
            // This will not ever shift by more than 63 bits.
            if (is_signed) {
                return 1ULL << (size_bytes * 8 - 1);
            } else {
                return 0;
            }
        }
    };

    struct FloatingType {
        bool is_double_precision;
        size_t size_bytes;
        std::string_view name;
    };

    struct PointerType {
        TypeId pointee_type;
        LifetimeId lifetime;
    };

    struct ConstType {
        TypeId const_type;
    };

    struct ArrayType {
        TypeId element_type;
        size_t size;
    };

    struct VoidType {};

    struct ErrorType {};

    using ProgramType = std::variant<StructType,
                                     IntegralType,
                                     FloatingType,
                                     PointerType,
                                     ConstType,
                                     ArrayType,
                                     VoidType,
                                     ErrorType>;

    class ResolvedType {
    public:
        ResolvedType() = default;
        ResolvedType(TypeId id, ProgramType program_type, TypeSymbol symbol, std::string_view name)
            : id(id), program_type(program_type), symbol(symbol), name(name) {}

        TypeId get_id() const { return id; }

        TypeSymbol get_symbol() const { return symbol; }

        bool is_void() const { return std::holds_alternative<VoidType>(program_type); }

        bool is_error() const { return std::holds_alternative<ErrorType>(program_type); }

        const ProgramType &get_program_type() const { return program_type; }

        std::string_view get_name() const { return name; }

        bool is_primitive() const;

    private:
        std::string_view name;
        TypeId id;
        TypeSymbol symbol;
        ProgramType program_type;
    };

    class TypeTable {
    public:
        static TypeTable builtin_type_table(const TypeSymbolRegistry &registry);

        TypeTable() = default;
        TypeTable(const TypeSymbolRegistry &registry) : registry(&registry) {}

        void add_type(ResolvedType type);

        void import(const TypeTable &other);

        TypeId get_type_id(TypeSymbol symbol) const { return registry->get_type_id(symbol); }

        std::vector<const ResolvedType *> get_types() const;

        ResolvedType get_type(TypeId id, const LifetimeGroup *lifetimes) const;

        ResolvedType get_type(const ast::Type *type, LifetimeGroup *lifetimes) const;

        ResolvedType get_named_type(NamedTypeSymbol named) const;

        bool operator==(const TypeTable &other) const;

    private:
        const TypeSymbolRegistry *registry = nullptr;
        std::unordered_map<TypeId, ResolvedType> types;
        mutable std::vector<TypeId> type_ids; // for equality checking
    };

    class TypeTableBuilder {
    public:
        TypeTableBuilder(const TypeSymbolRegistry *registry) : registry(registry) {}

        TypeTable build(const std::vector<arena::ast::Declaration *> &declarations) const;

    private:
        const TypeSymbolRegistry *registry;
    };

}; // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_TYPES_HPP