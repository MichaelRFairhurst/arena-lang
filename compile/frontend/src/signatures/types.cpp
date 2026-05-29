#include "signatures/types.hpp"

using namespace arena::sema;

TypeTable TypeTable::builtin_type_table(const TypeSymbolRegistry &registry) {
    std::unordered_map<std::string_view, ProgramType> builtin_types = {
        {"bool", IntegralType{true, 1, "bool"}},
        {"char", IntegralType{true, 1, "char"}},
        {"byte", IntegralType{false, 1, "byte"}},
        {"short", IntegralType{true, 2, "short"}},
        {"ushort", IntegralType{false, 2, "ushort"}},
        {"int", IntegralType{true, 4, "int"}},
        {"uint", IntegralType{false, 4, "uint"}},
        {"long", IntegralType{true, 8, "long"}},
        {"ulong", IntegralType{false, 8, "ulong"}},
        {"usize", IntegralType{false, 8, "usize"}},
        {"ssize", IntegralType{true, 8, "ssize"}},
        {"float32", FloatingType{false, 4, "float32"}},
        {"float64", FloatingType{true, 8, "float64"}},
        {"double32", FloatingType{false, 4, "double32"}},
        {"double64", FloatingType{true, 8, "double64"}},
    };

    TypeTable table{registry};
    auto void_id = table.registry.get_type_id(VoidTypeSymbol{});
    table.types.emplace(void_id,
                        ResolvedType{void_id, VoidType{}, VoidTypeSymbol{}, "<void>"});

    auto error_type_id = table.registry.get_type_id(ErrorTypeSymbol{});
    table.types.emplace(error_type_id,
                        ResolvedType{error_type_id, ErrorType{}, ErrorTypeSymbol{}, "<error>"});

    for (auto [name, type] : builtin_types) {
        auto symbol = NamedTypeSymbol{name};
        auto id = table.registry.get_type_id(symbol);
        table.types.emplace(id, ResolvedType{id, type, symbol, name});
    }

    return table;
}

std::vector<const ResolvedType *> TypeTable::get_types() const {
    std::vector<const ResolvedType *> result;
    for (const auto &entry : types) {
        result.push_back(&entry.second);
    }
    return result;
}

ResolvedType TypeTable::get_type(TypeId id) const {
    auto it = types.find(id);
    if (it == types.end()) {
        throw std::runtime_error("Type ID not found");
    }
    return it->second;
}