#include "types/substitute.hpp"

using namespace arena::sema;

namespace {
    class SubstituteLifetimeVisitor {
    public:
        SubstituteLifetimeVisitor(const TypeTable *ttable, const LifetimeGroup *type_lifetimes,
                                  const std::unordered_map<LifetimeId, LifetimeId> *substitutions)
            : ttable(ttable), type_lifetimes(type_lifetimes), substitutions(substitutions) {}

        TypeId substitute(TypeId type) { return substitute(ttable->get_type(type, type_lifetimes)); }
        TypeId substitute(ResolvedType type) {
            auto symbol = std::visit(*this, type.get_symbol());
            return ttable->get_type_id(symbol);
        }

        LifetimeId substitute(LifetimeId lifetime) {
            if (auto it = substitutions->find(lifetime); it != substitutions->end()) {
                return it->second;
            }
            return lifetime;
        }

        TypeSymbol operator()(const PointerTypeSymbol &symbol) {
            auto pointee = substitute(symbol.pointee_type);
            return PointerTypeSymbol{pointee, substitute(symbol.lifetime)};
        }

        TypeSymbol operator()(const ArrayTypeSymbol &array_type) {
            auto element = substitute(array_type.element_type);
            return ArrayTypeSymbol{element, array_type.size};
        }

        TypeSymbol operator()(const NamedTypeSymbol &named_type) { return named_type; }
        TypeSymbol operator()(const VoidTypeSymbol &void_type) { return void_type; }
        TypeSymbol operator()(const ErrorTypeSymbol &error_type) { return error_type; }

    private:
        const std::unordered_map<LifetimeId, LifetimeId> *substitutions;
        const TypeTable *ttable;
        const LifetimeGroup *type_lifetimes;
    };
}

TypeId arena::sema::substitute_lifetimes(
    TypeId type_id,
    const LifetimeGroup &type_lifetimes,
    const TypeTable *ttable,
    const std::unordered_map<LifetimeId, LifetimeId> &substitutions) {
    return SubstituteLifetimeVisitor{ttable, &type_lifetimes, &substitutions}.substitute(type_id);
}