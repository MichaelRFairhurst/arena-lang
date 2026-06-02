#include "signatures/types.hpp"
#include "ast/visitor.hpp"

using namespace arena::sema;
using namespace arena;

namespace {
    class TypeMaterializer {
    public:
        TypeMaterializer(const TypeTable *type_table, const TypeSymbolRegistry *registry)
            : type_table(type_table), registry(registry) {}

        ResolvedType operator()(const NamedTypeSymbol &symbol) {
            return type_table->get_named_type(symbol);
        }

        ResolvedType operator()(const ArrayTypeSymbol &symbol) {
            auto type_id = registry->get_type_id(symbol);
            auto element_id = symbol.element_type;
            auto element_type = type_table->get_type(element_id);

            // absolute royal freaking hack to intern the name into the type symbol
            std::string name = element_type.get_name().empty()
                                   ? "<unknown>"
                                   : std::string(element_type.get_name());
            name += "[" + std::to_string(symbol.size) + "]";
            auto tmp_symbol = registry->get_interned(NamedTypeSymbol{name});
            std::string_view interned_name = std::get<NamedTypeSymbol>(tmp_symbol).name;

            return ResolvedType{type_id, ArrayType{element_id, symbol.size}, symbol, interned_name};
        }

        ResolvedType operator()(const PointerTypeSymbol &symbol) {
            auto type_id = registry->get_type_id(symbol);
            auto pointee_id = symbol.pointee_type;
            auto pointee_type = type_table->get_type(pointee_id);

            std::string name = pointee_type.get_name().empty()
                                   ? "<unknown>"
                                   : std::string(pointee_type.get_name());
            name += "*";
            if (symbol.lifetime.has_value()) {
                name += " " + std::string(symbol.lifetime.value());
            }
            auto tmp_symbol = registry->get_interned(NamedTypeSymbol{name});
            std::string_view interned_name = std::get<NamedTypeSymbol>(tmp_symbol).name;

            return ResolvedType{type_id,
                                PointerType{pointee_id, symbol.lifetime},
                                symbol,
                                interned_name};
        }

        ResolvedType operator()(const VoidTypeSymbol &symbol) {
            auto type_id = registry->get_type_id(symbol);
            return ResolvedType{type_id, VoidType{}, symbol, "<void>"};
        }

        ResolvedType operator()(const ErrorTypeSymbol &symbol) {
            auto type_id = registry->get_type_id(symbol);
            return ResolvedType{type_id, ErrorType{}, symbol, "<error>"};
        }

    private:
        const TypeTable *type_table;
        const TypeSymbolRegistry *registry;
    };

    class TypeTableBuilderVisitor : public ast::Visitor {
    public:
        TypeTableBuilderVisitor(TypeTable *table, const TypeSymbolRegistry *registry)
            : table(table), registry(registry) {}

        void visit(const ast::StructDeclaration *struct_decl) {
            TypeSymbol symbol = registry->get_interned(NamedTypeSymbol{struct_decl->get_name()});
            TypeId id = registry->get_type_id(symbol);
            if (!std::holds_alternative<NamedTypeSymbol>(symbol)) {
                throw std::runtime_error("Expected a named type symbol for struct declaration");
            }
            auto name = std::get<NamedTypeSymbol>(symbol).name;

            StructType struct_type{name};
            ResolvedType resolved(id, struct_type, symbol, name);
            table->add_type(resolved);
        }

        void visit(const ast::StructDefinition *struct_def) {
            visit(static_cast<const ast::StructDeclaration *>(struct_def));
        }

        TypeTable *table;
        const TypeSymbolRegistry *registry;
    };

} // namespace

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
    auto void_id = table.registry->get_type_id(VoidTypeSymbol{});
    table.types.emplace(void_id, ResolvedType{void_id, VoidType{}, VoidTypeSymbol{}, "<void>"});

    auto error_type_id = table.registry->get_type_id(ErrorTypeSymbol{});
    table.types.emplace(error_type_id,
                        ResolvedType{error_type_id, ErrorType{}, ErrorTypeSymbol{}, "<error>"});

    for (auto [name, type] : builtin_types) {
        auto symbol = NamedTypeSymbol{name};
        auto id = table.registry->get_type_id(symbol);
        table.types.emplace(id, ResolvedType{id, type, symbol, name});
    }

    return table;
}

void TypeTable::add_type(ResolvedType type) {
    auto symbol = type.get_symbol();

    // Add to type_ids for equality if it's a named type.
    if (std::holds_alternative<NamedTypeSymbol>(symbol)) {
        type_ids.push_back(type.get_id());
    }

    types[type.get_id()] = type;
}

void TypeTable::import(const TypeTable &other) {
    for (const auto [id, type] : other.types) {
        if (types.find(id) != types.end()) {
            continue;
        }

        add_type(type);
    }
}

std::vector<const ResolvedType *> TypeTable::get_types() const {
    std::vector<const ResolvedType *> result;
    for (const auto &entry : types) {
        result.push_back(&entry.second);
    }
    return result;
}

ResolvedType TypeTable::get_type(TypeId id) const {
    TypeMaterializer materializer{this, registry};
    return std::visit(materializer, registry->get_type_symbol(id));
}

ResolvedType TypeTable::get_type(const ast::Type *type) const {
    TypeSymbolResolver symbol_resolver(registry);
    auto symbol = symbol_resolver.resolve(type);
    TypeMaterializer materializer{this, registry};
    return std::visit(materializer, symbol);
}

ResolvedType TypeTable::get_named_type(NamedTypeSymbol name) const {
    auto id = registry->get_type_id(name);

    auto it = types.find(id);
    if (it == types.end()) {
        throw std::runtime_error("Type ID not found for named type lookup");
    }
    return it->second;
}

bool TypeTable::operator==(const TypeTable &other) const {
    if (type_ids.size() != other.type_ids.size()) {
        return false;
    }

    std::sort(type_ids.begin(), type_ids.end(), [](auto &left, auto &right) {
        return left.t_id < right.t_id;
    });

    std::sort(other.type_ids.begin(), other.type_ids.end(), [](auto &left, auto &right) {
        return left.t_id < right.t_id;
    });

    auto [ours, theirs] = std::mismatch(type_ids.begin(),
                                        type_ids.end(),
                                        other.type_ids.begin(),
                                        other.type_ids.end());

    return ours == type_ids.end() && theirs == other.type_ids.end();
}

TypeTable TypeTableBuilder::build(
    const std::vector<arena::ast::Declaration *> &declarations) const {
    TypeTable result = TypeTable::builtin_type_table(*registry);
    TypeTableBuilderVisitor visitor{&result, registry};
    for (auto decl : declarations) {
        decl->accept(&visitor);
    }

    return result;
}