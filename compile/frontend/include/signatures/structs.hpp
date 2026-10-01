#ifndef ARENA_INCLUDE_SIGNATURES_STRUCTS_HPP
#define ARENA_INCLUDE_SIGNATURES_STRUCTS_HPP

#include "resolve/symbols.hpp"
#include "signatures/types.hpp"

namespace arena::sema {
    struct ResolvedStructMember {
        std::string_view name;
        TypeId type_id;

        bool operator==(const ResolvedStructMember &other) const {
            return name == other.name && type_id == other.type_id;
        }
        bool operator!=(const ResolvedStructMember &other) const { return !(*this == other); }
    };

    struct ResolvedStruct {
        StructId id;
        std::string_view name;
        size_t num_members;
        ResolvedStructMember *members;
        LifetimeGroup lifetimes;

        bool operator==(const ResolvedStruct &other) const {
            if (id != other.id)
                return false;
            if (name != other.name)
                return false;
            if (members != other.members)
                return false;
            for (size_t i = 0; i < num_members; ++i) {
                if (members[i] != other.members[i])
                    return false;
            }
            // TODO: Check lifetime equality
            // if (lifetimes != other.lifetimes) return false;
            return true;
        }

        bool operator!=(const ResolvedStruct &other) const { return !(*this == other); }
    };
} // namespace arena::sema

namespace arena::sema {
    class StructTable {
    public:
        StructTable() = default;
        StructTable(const StructSymbolRegistry &registry) : registry(&registry) {}

        void add_struct(ResolvedStruct strct) { structs[strct.id] = strct; }

        StructId get_struct_id(std::string_view name) const {
            return registry->get_struct_id(name);
        }

        std::optional<ResolvedStruct> get_struct(StructId struct_id) const {
            auto it = structs.find(struct_id);
            if (it != structs.end()) {
                return it->second;
            }

            return std::nullopt;
        }

        std::optional<ResolvedStruct> get_struct(std::string_view name) const {
            return get_struct(get_struct_id(name));
        }

        void import(const StructTable &other) {
            for (const auto &[key, value] : other.structs) {
                structs[key] = value;

                auto &strct = structs[key];
            }
        }

        ResolvedStructMember *alloc_members(size_t count) {
            return arena.alloc_array<ResolvedStructMember>(count);
        }

        bool operator==(const StructTable &other) const { return structs == other.structs; }
        bool operator!=(const StructTable &other) const { return !(*this == other); }

    private:
        util::Arena arena;
        const StructSymbolRegistry *registry = nullptr;
        std::unordered_map<StructId, ResolvedStruct> structs;
    };

    class StructTableBuilder {
    public:
        StructTableBuilder(const StructSymbolRegistry *registry, const TypeTable *type_table)
            : registry(registry), type_table(type_table) {}

        class StructTableBuilderVisitor : public arena::ast::Visitor {
        public:
            StructTableBuilderVisitor(const StructSymbolRegistry *registry,
                                      const TypeTable *type_table,
                                      StructTable *struct_table)
                : registry(registry), type_table(type_table), struct_table(struct_table) {}

            void visit(const arena::ast::StructDefinition *strct) override {
                TypeId struct_type_id = type_table->get_type_id(NamedTypeSymbol{strct->get_name()});
                StructId struct_id = registry->get_struct_id(strct->get_name());

                auto resolved_struct = ResolvedStruct{.id = struct_id, .name = strct->get_name()};
                auto fields = strct->get_fields();

                resolved_struct.num_members = fields->size();
                resolved_struct.members = struct_table->alloc_members(resolved_struct.num_members);
                auto resolved_member = resolved_struct.members;

                for (const auto &field : *strct->get_fields()) {
                    auto type = type_table->get_type(field->get_type(), &resolved_struct.lifetimes);
                    resolved_member->name = field->get_name();
                    resolved_member->type_id = type.get_id();

                    ++resolved_member;
                }

                struct_table->add_struct(resolved_struct);
            }

        private:
            const StructSymbolRegistry *registry;
            StructTable *struct_table;
            const TypeTable *type_table;
        };

        StructTable build(const std::vector<arena::ast::Declaration *> &declarations) const {
            StructTable table(*registry);
            StructTableBuilderVisitor visitor(registry, type_table, &table);
            for (const auto *decl : declarations) {
                decl->accept(&visitor);
            }
            return table;
        }

    private:
        const StructSymbolRegistry *registry;
        const TypeTable *type_table;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_SIGNATURES_STRUCTS_HPP