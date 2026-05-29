#ifndef ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP
#define ARENA_INCLUDE_RESOLVE_TYPECHECK_HPP

#include "resolve/tree.hpp"
#include "resolve/tree_transform.hpp"
#include "resolve/expressions.hpp"

namespace arena::sema {

    class TypeChecker {
    public:
        TypeChecker(const FunctionTable &ftable, const TypeTable &ttable) : ftable(&ftable), ttable(&ttable) {}

        ResolvedExpressionsResult type_check(const std::vector<const ResolvedDeclaration *> &decls);

    private:
        const FunctionTable *ftable;
        const TypeTable *ttable;
    };

} // namespace arena::sema


#endif