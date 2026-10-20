#include "types/inference.hpp"
#include "types/operations.hpp"
#include <cassert>

using namespace arena;
using namespace arena::sema;

void InferenceContext::constrain_context_type(VariableId id) {
    auto variable = ops->get_variables().resolve_variable(id);

    if (variable->explicit_type_id.has_value()) {
        constrain_context_type(variable->explicit_type_id.value(),
                               error::LocatedText{variable->declaration,
                                                  "variable '" + std::string(variable->name) +
                                                      "'"});
    } else if (variable->inferred_type_id.has_value()) {
        constrain_context_type(variable->inferred_type_id.value(),
                               error::LocatedText{variable->declaration,
                                                  "variable '" + std::string(variable->name) +
                                                      "'"});
    } else if (context_type.has_value()) {
        variable->inferred_type_id = *context_type;
    } else {
        ops->get_errors().E_T_CANT_INFER_V(variable->declaration, variable->name);
    }
}

void InferenceContext::constrain_context_type(TypeId id,
                                              error::LocatedText why,
                                              ConstraintKind kind) {
    if (context_type.has_value() && kind == ConstraintKind::Suggestion) {
        // If the current context type is already set and the new constraint is only a suggestion,
        // we do not override the existing context type.
        return;
    } else if (!context_type.has_value() || context_kind == ConstraintKind::Suggestion) {
        context_type = id;
        context_kind = kind;
        why_constraint = why;
        return;
    }

    auto err = ops->require_assignable(*context_type, id, context_node, why.second);

    if (err != nullptr) {
        err->add_supplement(error::SupplementKind::Note,
                            "expected type " + ops->get_type_name(*context_type) + " based on " +
                                why_constraint.second,
                            why_constraint.first);
        err->add_supplement(error::SupplementKind::Note,
                            "got conflicting type " + ops->get_type_name(id) + " for " + why.second,
                            why.first);
    }
}

void InferenceContext::constrain_dereferences(InferenceContext *other,
                                              const ast::Node *origin,
                                              ConstraintKind kind) {
    if (other == nullptr) {
        return;
    }

    auto other_context_type = other->context_type;
    if (!other_context_type.has_value()) {
        return;
    }

    auto dereferenced_type = ops->dereference(*other_context_type);
    if (!dereferenced_type.has_value()) {
        ops->get_errors().E_T_CANT_DEREF(origin,
                                         {other->context_node,
                                          ops->get_type_name(*other_context_type)});
        dereferenced_type = ops->get_error_type();
    }

    constrain_context_type(dereferenced_type->get_id(),
                           error::LocatedText{origin, "dereferenced type"},
                           kind);
}

void InferenceContext::constrain_points_to(InferenceContext *other,
                                           LifetimeId lifetime,
                                           const ast::Node *origin,
                                           ConstraintKind kind) {
    if (other == nullptr) {
        return;
    }

    auto other_context_type = other->context_type;
    if (!other_context_type.has_value()) {
        return;
    }

    auto pointer_to_other =
        ops->get_types().get_type_id(PointerTypeSymbol{*other_context_type, lifetime});
    auto link = error::LocatedText{origin, "here"};
    constrain_context_type(pointer_to_other, link, kind);
}

void InferenceContext::constrain_indexes_into(InferenceContext *other,
                                              const ast::Node *origin,
                                              ConstraintKind kind) {
    if (other == nullptr) {
        return;
    }

    auto other_context_type = other->context_type;
    if (!other_context_type.has_value()) {
        return;
    }

    auto element_type = ops->index_into(*other_context_type);
    if (!element_type.has_value()) {
        ops->get_errors().E_T_CANT_INDEX(origin,
                                         {other->context_node,
                                          ops->get_type_name(*other_context_type)});
        element_type = ops->get_error_type();
    }

    constrain_context_type(element_type->get_id(),
                           error::LocatedText{origin, "element type"},
                           kind);
}

void InferenceContext::constrain_indexable_as(InferenceContext *other, const ast::Node *origin) {
    if (other == nullptr) {
        return;
    }

    auto other_context_type = other->context_type;
    if (!other_context_type.has_value()) {
        return;
    }

    auto array_of_other = ops->get_types().get_type_id(ArrayTypeSymbol{*other_context_type});
    auto link = error::LocatedText{origin, "here"};
    constrain_context_type(array_of_other, link, ConstraintKind::Suggestion);
}

void InferenceContext::constrain_int_literal(const ast::Node *literal) {
    if (!context_type.has_value()) {
        // If the context type is not yet constrained, suggest it to be an int.
        constrain_context_type(ops->get_types().get_type_id(NamedTypeSymbol{"int"}),
                               error::LocatedText{literal, "integer literal"},
                               ConstraintKind::Suggestion);
        return;
    }

    if (context_kind == ConstraintKind::Assignee) {
        // If the context has a hard constraint, defer to that hard constraint.
        return;
    }

    assert(context_type.has_value());
    assert(context_kind == ConstraintKind::Suggestion);
    if (!ops->is_integral(context_type.value(), IntegralLiteralKind::Int)) {
        // If the context is suggesting a non-int type, override it with a suggestion of type int.
        context_type = ops->get_types().get_type_id(NamedTypeSymbol{"int"});
    }
}

TypeId InferenceContext::get_inferred_context_type() {
    if (context_type.has_value()) {
        return *context_type;
    }

    ops->get_errors().E_T_CANT_INFER_EX(context_node);
    return ops->get_types().get_type_id(ErrorTypeSymbol{});
}