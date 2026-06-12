#include "types/inference.hpp"
#include "types/typecheck.hpp"

using namespace arena;
using namespace arena::sema;

void InferenceContext::constrain_context_type(VariableId id) {
    auto variable = ops->get_variables().resolve_variable(id);

    if (variable->explicit_type_id.has_value()) {
        constrain_context_type(variable->explicit_type_id.value(),
                               error::Link{variable->declaration,
                                           "variable '" + std::string(variable->name) + "'"});
    } else if (variable->inferred_type_id.has_value()) {
        constrain_context_type(variable->inferred_type_id.value(),
                               error::Link{variable->declaration,
                                           "variable '" + std::string(variable->name) + "'"});
    } else if (context_type.has_value()) {
        variable->inferred_type_id = *context_type;
    } else {
        ops->get_errors().report(variable->declaration,
                                 "Cannot infer type for variable '" + std::string(variable->name) +
                                     "': no constraints on variable and context type is unknown");
    }
}

void InferenceContext::constrain_context_type(TypeId id, error::Link why) {
    if (!context_type.has_value()) {
        context_type = id;
        return;
    }

    ops->require_assignable(*context_type,
                            id,
                            context_node,
                            "Provided type does not match expected");
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
        constrain_context_type(pointee_id, error::Link{origin, "dereferenced type"});
        return;
    }

    auto error_type = ops->get_types().get_type_id(ErrorTypeSymbol{});
    constrain_context_type(error_type, error::Link{origin, "dereferenced type"});
    if (other_context_type == error_type) {
        return;
    }

    ops->get_errors().report(origin,
                             "Expected a dereferenceable type, but got '" +
                                 ops->get_type_name(*other_context_type) + "'");
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
    auto link = error::Link{origin};
    constrain_context_type(pointer_to_other, link);
}

TypeId InferenceContext::get_inferred_context_type() {
    if (context_type.has_value()) {
        return *context_type;
    }

    ops->get_errors().report(context_node,
                             "Could not infer type of expression: insufficient type information");
    return ops->get_types().get_type_id(ErrorTypeSymbol{});
}