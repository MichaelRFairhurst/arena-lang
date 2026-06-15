#ifndef ARENA_INCLUDE_TYPES_INFERENCE_HPP
#define ARENA_INCLUDE_TYPES_INFERENCE_HPP

#include "resolve/variables.hpp"
#include "signatures/types.hpp"
#include "errors/errors.hpp"

namespace arena::sema {
    class TypecheckOperations;

    class InferenceContext {
    public:
        InferenceContext(const ast::Node *context_node, TypecheckOperations *ops) : context_node(context_node), ops(ops) {}

        ~InferenceContext() = default;
        void constrain_context_type(sema::VariableId id);
        void constrain_context_type(sema::TypeId id, error::LocatedText why);
        void constrain_dereferences(InferenceContext *other, const ast::Node *origin);
        void constrain_points_to(InferenceContext *other,
                                 LifetimeId lifetime,
                                 const ast::Node *origin);
        sema::TypeId get_inferred_context_type();

    private:
        std::optional<TypeId> context_type;
        const ast::Node *context_node = nullptr;
        error::LocatedText why_constraint;
        TypecheckOperations *ops;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_TYPES_INFERENCE_HPP