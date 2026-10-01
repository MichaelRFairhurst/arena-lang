#ifndef ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP
#define ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP

#include "resolve/tree.hpp"
#include "resolve/tree_transform.hpp"
#include "resolve/expressions.hpp"
#include "signatures/structs.hpp"

namespace arena::sema {

    class TypeChecker {
    public:
        TypeChecker(const FunctionTable &ftable,
                    const TypeTable &ttable,
                    const StructTable &struct_table)
            : ftable(&ftable), ttable(&ttable), struct_table(&struct_table) {}

        ResolvedExpressionsResult type_check(const std::vector<const ResolvedDeclaration *> &decls,
                                             const VariableRegistry *registry);

    private:
        const FunctionTable *ftable;
        const TypeTable *ttable;
        const StructTable *struct_table;
    };

} // namespace arena::sema


#endif