#ifndef ARENA_INCLUDE_TYPES_INFERENCE_HPP
#define ARENA_INCLUDE_TYPES_INFERENCE_HPP

#include "resolve/variables.hpp"
#include "signatures/types.hpp"
#include "errors/errors.hpp"

namespace arena::sema {
    class InferenceContext {
    public:
        InferenceContext(const VariableRegistry *registry,
                         const TypeTable *ttable,
                         const ast::Node *context_node,
                         error::Reporter *errors)
            : registry(registry), ttable(ttable), context_node(context_node), errors(errors) {}

        ~InferenceContext() = default;
        void constrain_context_type(sema::VariableId id);
        void constrain_context_type(sema::TypeId id, error::Link why);
        void constrain_dereferences(InferenceContext *other, const ast::Node *origin);
        void constrain_points_to(InferenceContext *other, const ast::Node *origin);
        sema::TypeId get_inferred_context_type();

    private:
        std::optional<TypeId> context_type;
        const ast::Node *context_node = nullptr;
        const VariableRegistry *registry;
        const TypeTable *ttable;
        error::Reporter *errors;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_TYPES_INFERENCE_HPP