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

    struct CompleteStructInfo {
        size_t num_members;
        ResolvedStructMember *members;
        LifetimeGroup lifetimes;
        std::vector<const ast::NamedType *> type_references;

        bool operator==(const CompleteStructInfo &other) const {
            for (size_t i = 0; i < num_members; ++i) {
                if (members[i] != other.members[i])
                    return false;
            }
            // TODO: Check lifetime equality
            // if (lifetimes != other.lifetimes) return false;
            return true;
        }

        bool operator!=(const CompleteStructInfo &other) const { return !(*this == other); }
    };

    struct ResolvedStruct {
        StructId id;
        std::string_view name;
        std::optional<CompleteStructInfo> complete_info;

        bool operator==(const ResolvedStruct &other) const {
            return id == other.id && name == other.name && complete_info == other.complete_info;
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
        StructTableBuilder(const StructSymbolRegistry *registry,
                           const TypeSymbolRegistry *type_registry)
            : registry(registry), type_registry(type_registry) {}

        class StructTableBuilderVisitor : public arena::ast::Visitor {
        public:
            StructTableBuilderVisitor(const StructSymbolRegistry *registry,
                                      const TypeSymbolRegistry *type_registry,
                                      StructTable *struct_table)
                : registry(registry), type_registry(type_registry), struct_table(struct_table) {}

            void visit(const arena::ast::StructDefinition *strct) override {
                TypeId struct_type_id =
                    type_registry->get_type_id(NamedTypeSymbol{strct->get_name()});
                StructId struct_id = registry->get_struct_id(strct->get_name());

                auto resolved_struct = ResolvedStruct{.id = struct_id, .name = strct->get_name()};
                auto struct_info = CompleteStructInfo{};
                auto fields = strct->get_fields();

                struct_info.num_members = fields->size();
                struct_info.members = struct_table->alloc_members(struct_info.num_members);
                auto resolved_member = struct_info.members;

                LifetimeTable lifetime_table{&struct_info.lifetimes, true};
                TypeSymbolResolver type_symbolizer{type_registry, &lifetime_table};
                for (const auto &field : *strct->get_fields()) {
                    auto type_id = type_registry->get_type_id(
                        type_symbolizer.resolve(field->get_type(), &struct_info.type_references));
                    resolved_member->name = field->get_name();
                    resolved_member->type_id = type_id;

                    ++resolved_member;
                }

                resolved_struct.complete_info = struct_info;
                struct_table->add_struct(resolved_struct);
            }

        private:
            const StructSymbolRegistry *registry;
            StructTable *struct_table;
            const TypeSymbolRegistry *type_registry;
        };

        StructTable build(const std::vector<arena::ast::Declaration *> &declarations) const {
            StructTable table(*registry);
            StructTableBuilderVisitor visitor(registry, type_registry, &table);
            for (const auto *decl : declarations) {
                decl->accept(&visitor);
            }
            return table;
        }

    private:
        const StructSymbolRegistry *registry;
        const TypeSymbolRegistry *type_registry;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_SIGNATURES_STRUCTS_HPP