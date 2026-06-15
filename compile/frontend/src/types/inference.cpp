#include "types/inference.hpp"
#include "types/typecheck.hpp"

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

void InferenceContext::constrain_context_type(TypeId id, error::LocatedText why) {
    if (!context_type.has_value()) {
        context_type = id;
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
                            "got conflicting type " + ops->get_type_name(id) + " for " +
                                why.second,
                            why.first);
    }
}

void InferenceContext::constrain_dereferences(InferenceContext *other, const ast::Node *origin) {
    if (other == nullptr) {
        return;
    }

    auto other_context_type = other->context_type;
    if (!other_context_type.has_value()) {
        return;
    }

    auto type = ops->get_type(*other_context_type);
    if (auto ptr = std::get_if<PointerType>(&type.get_program_type())) {
        auto pointee_id = ptr->pointee_type;
        constrain_context_type(pointee_id, error::LocatedText{origin, "dereferenced type"});
        return;
    }

    auto error_type = ops->get_types().get_type_id(ErrorTypeSymbol{});
    constrain_context_type(error_type, error::LocatedText{origin, "dereferenced type"});
    if (other_context_type == error_type) {
        return;
    }

    ops->get_errors().E_T_CANT_DEREF(origin,
                                     {other->context_node,
                                      ops->get_type_name(*other_context_type)});
}

void InferenceContext::constrain_points_to(InferenceContext *other,
                                           LifetimeId lifetime,
                                           const ast::Node *origin) {
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
    constrain_context_type(pointer_to_other, link);
}

TypeId InferenceContext::get_inferred_context_type() {
    if (context_type.has_value()) {
        return *context_type;
    }

    ops->get_errors().E_T_CANT_INFER_EX(context_node);
    return ops->get_types().get_type_id(ErrorTypeSymbol{});
}