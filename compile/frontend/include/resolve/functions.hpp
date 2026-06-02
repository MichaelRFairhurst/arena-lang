#ifndef ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP
#define ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP

#include <vector>
#include <unordered_map>
#include "ast/declarations.hpp"
#include "parse/parse.hpp"
#include "ast/visitor.hpp"
#include "resolve/symbols.hpp"
#include "signatures/types.hpp"
#include "signatures/functions.hpp"

namespace arena::sema {

    class FunctionTableBuilder {
    public:
        FunctionTableBuilder(const FunctionSymbolRegistry &registry,
                             const TypeSymbolRegistry &type_registry,
                             const TypeTable &type_table)
            : registry(&registry), type_registry(&type_registry), type_table(&type_table) {}

        FunctionTable build(const std::vector<arena::ast::Declaration *> &declarations) const;

    private:
        const FunctionSymbolRegistry *registry;
        const TypeSymbolRegistry *type_registry;
        const TypeTable *type_table;
    };

} // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_FUNCTIONS_HPP