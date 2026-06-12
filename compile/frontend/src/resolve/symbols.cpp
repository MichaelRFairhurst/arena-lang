#include "resolve/symbols.hpp"
#include "signatures/functions.hpp"
#include "signatures/types.hpp"
#include "signatures/lifetimes.hpp"
#include <iostream>

using namespace arena::sema;
using namespace arena;

namespace {
    class TypeSymbolResolverVisitor : public ast::Visitor {
    public:
        TypeSymbolResolverVisitor(const TypeSymbolRegistry &registry, LifetimeTable *lifetimes)
            : registry(&registry), lifetimes(lifetimes) {}

        void visit(const ast::NamedType *named_type) override {
            result = registry->get_interned(NamedTypeSymbol{named_type->get_name()});
        }

        void visit(const ast::ArrayType *array_type) override {
            array_type->get_element_type()->accept(this);
            auto element_id = registry->get_type_id(result);
            // TODO: use size literal
            // auto size_literal = array_type->get_size_literal();
            result = registry->get_interned(ArrayTypeSymbol{element_id, 10});
        }

        void visit(const ast::ConstType *const_type) override {
            throw std::runtime_error("Const types are not supported in type symbols yet");
        }

        void visit(const ast::PointerType *pointer_type) override {
            pointer_type->get_pointee()->accept(this);
            auto pointee_id = registry->get_type_id(result);
            auto explicit_lifetime = pointer_type->get_lifetime();
            LifetimeId lifetime_id;
            if (explicit_lifetime.has_value()) {
                lifetime_id = lifetimes->lookup(explicit_lifetime.value(), pointer_type);
            } else {
                lifetime_id = lifetimes->infer_lifetime(pointer_type);
            }

            result = registry->get_interned(PointerTypeSymbol{pointee_id, lifetime_id});
        }

        TypeSymbol get_result() const { return result; }

    protected:
        const TypeSymbolRegistry *registry;
        TypeSymbol result;
        LifetimeTable *lifetimes;
    };

}; // namespace

FunctionSymbolSet::FunctionSymbolSet(const FunctionTable *ftable) {
    auto functions = ftable->get_functions();
    for (const auto &function : ftable->get_functions()) {
        symbol_to_id[function->get_symbol()] = function->get_id();
        sorted_ids.push_back(function->get_id());
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const FunctionId &a, const FunctionId &b) {
        return a.f_id < b.f_id;
    });
}

void FunctionSymbolSet::import(const FunctionSymbolSet &other) {
    for (const auto &entry : other.symbol_to_id) {
        symbol_to_id[entry.first] = entry.second;
        sorted_ids.push_back(entry.second);
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const FunctionId &a, const FunctionId &b) {
        return a.f_id < b.f_id;
    });
}

std::optional<FunctionId> FunctionSymbolSet::get_id(FunctionSymbol symbol) const {
    auto it = symbol_to_id.find(symbol);
    if (it != symbol_to_id.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool FunctionSymbolSet::operator==(const FunctionSymbolSet &other) const {
    if (symbol_to_id.size() != other.symbol_to_id.size()) {
        return false;
    }

    auto [our_end, their_end] =
        std::mismatch(sorted_ids.begin(), sorted_ids.end(), other.sorted_ids.begin());
    if (our_end != sorted_ids.end() || their_end != other.sorted_ids.end()) {
        return false;
    }

    return true;
}

TypeSymbolSet::TypeSymbolSet(const TypeTable *ttable, const TypeSymbolRegistry &registry)
    : registry(&registry) {
    for (const auto &type : ttable->get_types()) {
        symbol_to_id[type->get_symbol()] = type->get_id();
        sorted_ids.push_back(type->get_id());
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const TypeId &a, const TypeId &b) {
        return a.t_id < b.t_id;
    });
}

void TypeSymbolSet::import(const TypeSymbolSet &other) {
    for (const auto &entry : other.symbol_to_id) {
        symbol_to_id[entry.first] = entry.second;
        sorted_ids.push_back(entry.second);
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const TypeId &a, const TypeId &b) {
        return a.t_id < b.t_id;
    });
}

std::optional<TypeId> TypeSymbolSet::get_id(TypeSymbol symbol) const {
    if (auto it = symbol_to_id.find(symbol); it != symbol_to_id.end()) {
        return it->second;
    }

    if (!is_available(symbol)) {
        return std::nullopt;
    }

    auto id = registry->get_type_id(symbol);
    symbol_to_id[symbol] = id;
    return id;
}

bool TypeSymbolSet::is_available(TypeSymbol symbol) const {
    return std::visit(
        [this](auto &&s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, NamedTypeSymbol>) {
                return symbol_to_id.find(s) != symbol_to_id.end();
            } else if constexpr (std::is_same_v<T, PointerTypeSymbol>) {
                auto pointee_id = s.pointee_type;
                auto pointee_symbol = registry->get_type_symbol(pointee_id);
                return is_available(pointee_symbol);
            } else if constexpr (std::is_same_v<T, ArrayTypeSymbol>) {
                auto element_id = s.element_type;
                auto element_symbol = registry->get_type_symbol(element_id);
                return is_available(element_symbol);
            } else if constexpr (std::is_same_v<T, VoidTypeSymbol>) {
                return true;
            } else if constexpr (std::is_same_v<T, ErrorTypeSymbol>) {
                return true;
            } else {
                static_assert(false && sizeof(T), "Missing type in visit handler.");
            }
        },
        symbol);
}

bool TypeSymbolSet::operator==(const TypeSymbolSet &other) const {
    if (symbol_to_id.size() != other.symbol_to_id.size()) {
        return false;
    }

    auto [our_end, their_end] =
        std::mismatch(sorted_ids.begin(), sorted_ids.end(), other.sorted_ids.begin());
    if (our_end != sorted_ids.end() || their_end != other.sorted_ids.end()) {
        return false;
    }

    return true;
}

TypeSymbol TypeSymbolResolver::resolve(const ast::Type *type) const {
    TypeSymbolResolverVisitor visitor(*registry, lifetimes);
    type->accept(&visitor);
    return visitor.get_result();
}