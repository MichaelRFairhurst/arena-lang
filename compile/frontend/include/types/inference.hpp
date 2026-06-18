#ifndef ARENA_INCLUDE_TYPES_INFERENCE_HPP
#define ARENA_INCLUDE_TYPES_INFERENCE_HPP

#include "resolve/variables.hpp"
#include "signatures/types.hpp"
#include "errors/errors.hpp"

namespace arena::sema {
    class TypeOperations;

    class InferenceContext {
    public:
        enum class ConstraintKind {
            Assignee,
            Suggestion,
        };

        InferenceContext(const ast::Node *context_node, TypeOperations *ops)
            : context_node(context_node), ops(ops) {}

        ~InferenceContext() = default;
        void constrain_context_type(sema::VariableId id);
        void constrain_context_type(sema::TypeId id,
                                    error::LocatedText why,
                                    ConstraintKind kind = ConstraintKind::Assignee);
        void constrain_dereferences(InferenceContext *other,
                                    const ast::Node *origin,
                                    ConstraintKind kind = ConstraintKind::Assignee);
        void constrain_points_to(InferenceContext *other,
                                 LifetimeId lifetime,
                                 const ast::Node *origin,
                                 ConstraintKind kind = ConstraintKind::Assignee);
        sema::TypeId get_inferred_context_type();

    private:
        std::optional<TypeId> context_type;
        const ast::Node *context_node = nullptr;
        error::LocatedText why_constraint;
        ConstraintKind context_kind;
        TypeOperations *ops;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_TYPES_INFERENCE_HPP