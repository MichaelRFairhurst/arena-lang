#include "resolve/functions.hpp"

namespace {
    using namespace arena::sema;
    class FunctionTableBuilderVisitor : public arena::ast::Visitor {
    public:
        FunctionTableBuilderVisitor(const FunctionSymbolRegistry &fregistry,
                                    const TypeTable &type_table,
                                    FunctionTable &ftable,
                                    const TypeSymbolRegistry &type_registry)
            : ftable(&ftable), type_table(&type_table), fregistry(&fregistry),
              type_registry(&type_registry) {}

        void visit(const arena::ast::FunctionDeclaration *decl) override {
            auto symbol = FunctionSymbol{decl->get_name_token()->text};
            LifetimeGroup lifetimes;
            LifetimeTable lt_table{&lifetimes, true};
            auto id = fregistry->get_function_id(symbol);
            symbol = fregistry->get_function_symbol(id); // Get interned symbol
            std::vector<TypeId> param_types;

            TypeSymbolResolver symbolizer(type_registry, &lt_table);
            auto args = decl->get_params()->get_params();
            for (auto arg : args) {
                auto arg_type = arg->get_type();
                auto arg_type_id = type_table->get_type_id(symbolizer.resolve(arg_type));
                param_types.push_back(arg_type_id);
            }

            std::optional<TypeId> return_type_id;
            if (decl->get_return_type()) {
                return_type_id =
                    type_table->get_type_id(symbolizer.resolve(decl->get_return_type()));
            }

            ResolvedFunction function{id,
                                      symbol,
                                      param_types,
                                      return_type_id,
                                      std::move(lifetimes)};
            ftable->add_function(symbol, function, decl);
        }

        void visit(const arena::ast::FunctionDefinition *def) override {
            // Function definitions are also function declarations, so we can reuse the same logic.
            visit(static_cast<const arena::ast::FunctionDeclaration *>(def));
        }

    private:
        FunctionTable *ftable;
        const TypeTable *type_table;
        const TypeSymbolRegistry *type_registry;
        const FunctionSymbolRegistry *fregistry;
    };
} // namespace

FunctionTable arena::sema::FunctionTableBuilder::build(
    const std::vector<arena::ast::Declaration *> &declarations) const {
    FunctionTable ftable(*registry);
    FunctionTableBuilderVisitor visitor(*registry, *type_table, ftable, *type_registry);
    for (auto decl : declarations) {
        decl->accept(&visitor);
    }
    return ftable;
}