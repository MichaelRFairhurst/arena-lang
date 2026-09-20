#include "arena_llvm_types.hpp"

namespace {
    struct ResolvedTypeLLVMVisitor {
        ::llvm::IRBuilder<> *builder;
        arena::llvm::TypeResolver *resolver;

        ::llvm::Type *operator()(const arena::sema::VoidType &) const { return builder->getVoidTy(); }

        ::llvm::Type *operator()(const arena::sema::IntegralType &i) const {
            return builder->getIntNTy(i.size_bytes * 8);
        }

        ::llvm::Type *operator()(const arena::sema::FloatingType &f) const {
            switch (f.size_bytes) {
            case 4:
                return builder->getFloatTy();
            case 8:
                return builder->getDoubleTy();
            default:
                return nullptr;
            }
        }

        ::llvm::Type *operator()(const arena::sema::StructType &) const {
            throw std::runtime_error("StructType not yet supported in LLVM type resolution");
        }

        ::llvm::Type *operator()(const arena::sema::PointerType &p) const { return builder->getPtrTy(); }

        ::llvm::Type *operator()(const arena::sema::ConstType &c) const {
            return resolver->getLLVMType(c.const_type);
        }

        ::llvm::Type *operator()(const arena::sema::ArrayType &a) const {
            auto element_type = resolver->getLLVMType(a.element_type);
            if (!element_type) {
                return nullptr;
            }
            return ::llvm::ArrayType::get(element_type, a.size);
        }

        ::llvm::Type *operator()(const arena::sema::ErrorType &) const {
            throw std::runtime_error("ErrorType cannot be converted to an LLVM type");
        }
    };

} // namespace

::llvm::Type *arena::llvm::TypeResolver::getLLVMType(const arena::sema::TypeId type_id) {
    auto type = ttable->get_type(type_id, lifetimes);
    return std::visit(ResolvedTypeLLVMVisitor{builder, this}, type.get_program_type());
}