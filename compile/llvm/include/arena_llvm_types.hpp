#ifndef ARENA_LLVM_INCUDE_ARENA_LLVM_TYPES_HPP
#define ARENA_LLVM_INCUDE_ARENA_LLVM_TYPES_HPP

#include "llvm/IR/IRBuilder.h"
#include "signatures/types.hpp"

namespace arena::llvm {
    class TypeResolver {
    public:
        TypeResolver(::llvm::IRBuilder<> *builder,
                     const arena::sema::TypeTable *ttable,
                     const arena::sema::LifetimeGroup *lifetimes)
            : builder(builder), ttable(ttable), lifetimes(lifetimes) {}

        ::llvm::Type *getLLVMType(const arena::sema::TypeId type_id);

    private:
        ::llvm::IRBuilder<> *builder;
        const arena::sema::LifetimeGroup *lifetimes;
        const arena::sema::TypeTable *ttable;
    };

} // namespace arena::llvm

#endif // ARENA_LLVM_INCUDE_ARENA_LLVM_TYPES_HPP