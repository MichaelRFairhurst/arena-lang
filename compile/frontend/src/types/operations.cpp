#include "types/operations.hpp"
#include "types/substitute.hpp"

using namespace arena::sema;
using namespace arena;

namespace {
    class DereferenceVisitor {
    public:
        DereferenceVisitor(const TypeOperations *ops) : ops(ops) {}

        std::optional<ResolvedType> operator()(PointerType ptr_type) const {
            return ops->get_type(ptr_type.pointee_type);
        }

        std::optional<ResolvedType> operator()(ConstType const_type) const {
            auto consted = ops->get_type(const_type.const_type);
            auto inner = std::visit(*this, consted.get_program_type());
            if (inner == std::nullopt) {
                return std::nullopt;
            }

            if (std::holds_alternative<ConstType>(inner->get_program_type()) ||
                std::holds_alternative<ErrorType>(inner->get_program_type())) {
                return inner;
            }

            auto inner_type_id = inner->get_id();
            return ops->get_type(ops->get_types().get_type_id(ConstTypeSymbol{inner_type_id}));
        }

        std::optional<ResolvedType> operator()(ErrorType error_type) const {
            return ops->get_error_type();
        }

        template <typename CannotDeref>
        std::optional<ResolvedType> operator()(CannotDeref type) const {
            return std::nullopt;
        }

    private:
        const TypeOperations *ops;
    };
} // namespace

ResolvedType TypeOperations::get_type(TypeId id) const { return ttable->get_type(id, lifetimes); }

std::string TypeOperations::get_type_name(TypeId id) const {
    return std::string(get_type(id).get_name());
}

ResolvedType TypeOperations::get_error_type() const {
    return ttable->get_type(ttable->get_type_id(ErrorTypeSymbol{}), lifetimes);
}

std::string TypeOperations::get_type_name(TypeId id, const LifetimeGroup &type_lifetimes) const {
    return std::string(ttable->get_type(id, &type_lifetimes).get_name());
}

std::optional<ResolvedType> TypeOperations::dereference(TypeId id) const {
    return std::visit(DereferenceVisitor(this), get_type(id).get_program_type());
}

std::optional<LifetimeId> TypeOperations::pointed_lifetime(TypeId id) const {
    if (auto ptr_type = std::get_if<PointerType>(&get_type(id).get_program_type())) {
        return ptr_type->lifetime;
    } else if (auto const_type = std::get_if<ConstType>(&get_type(id).get_program_type())) {
        return pointed_lifetime(const_type->const_type);
    }

    return std::nullopt;
}

bool TypeOperations::is_const_strict(TypeId id) const {
    // const-strict types are types that are essentially shallow copied, so constness must be
    // preserved on copy to prevent accidental mutation.
    auto type = get_type(id);

    if (type.is_primitive() || type.is_void() || type.is_error()) {
        return false;
    }

    if (std::holds_alternative<ConstType>(type.get_program_type())) {
        return false;
    }

    if (auto ptr = std::get_if<PointerType>(&type.get_program_type())) {
        auto pointee = get_type(ptr->pointee_type);
        return !std::holds_alternative<ConstType>(pointee.get_program_type());
    }

    if (auto array = std::get_if<ArrayType>(&type.get_program_type())) {
        return is_const_strict(array->element_type);
    }

    // TODO: Handle const strictness for structs based on their fields.
    return true;
}

bool TypeOperations::is_lifetime_strict(TypeId id) const {
    // lifetime-strict types are types that contain writable pointers with unfixed lifetimes, such
    // that casting a value of lifetime `*a` to a shorter lifetime `*b` would allow smuggling a
    // shorter lived object into that pointer and outliving it.
    auto type = get_type(id);

    if (type.is_primitive() || type.is_void() || type.is_error()) {
        return false;
    }

    if (std::holds_alternative<ConstType>(type.get_program_type())) {
        return false;
    }

    if (auto ptr = std::get_if<PointerType>(&type.get_program_type())) {
        auto pointee = get_type(ptr->pointee_type);
        return !std::holds_alternative<ConstType>(pointee.get_program_type());
    }

    if (auto array = std::get_if<ArrayType>(&type.get_program_type())) {
        return is_lifetime_strict(array->element_type);
    }

    // TODO: Handle lifetime strictness for structs based on their fields.
    return true;
}

TypeId TypeOperations::substitute_lifetimes(
    TypeId type_id,
    const LifetimeGroup &type_lifetimes,
    const std::unordered_map<LifetimeId, LifetimeId> &substitutions) const {
    return arena::sema::substitute_lifetimes(type_id, type_lifetimes, ttable, substitutions);
}

error::Error *TypeOperations::require_assignable(TypeId lhs,
                                                 TypeId rhs,
                                                 const ast::Node *node,
                                                 std::string message,
                                                 assignment_context context) {
    if (lhs == rhs) {
        return nullptr;
    }

    auto lhs_type = get_type(lhs);
    auto rhs_type = get_type(rhs);
    if (lhs_type.is_error() || rhs_type.is_error()) {
        // Don't report spurious errors if one of the types is already an error
        return nullptr;
    }

    auto lhs_const = std::get_if<ConstType>(&lhs_type.get_program_type());
    auto rhs_const = std::get_if<ConstType>(&rhs_type.get_program_type());

    if (lhs_const && rhs_const) {
        auto err = require_assignable(lhs_const->const_type,
                                      rhs_const->const_type,
                                      node,
                                      message,
                                      {.is_copy = context.is_copy, .is_const = true});
        if (err != nullptr) {
            err->add_cause("Const type mismatch",
                           "Const-qualified type '" + get_type_name(rhs_const->const_type) +
                               "' must be compatible with const-qualified type '" +
                               get_type_name(lhs_const->const_type) + "'.");
        }

        return err;
    } else if (lhs_const) {
        // x const = x is always allowed.
        auto err = require_assignable(lhs_const->const_type,
                                      rhs,
                                      node,
                                      message,
                                      {.is_copy = context.is_copy, .is_const = true});
        if (err != nullptr) {
            err->add_cause("Const type mismatch",
                           "Type '" + get_type_name(rhs_const->const_type) +
                               "' must be compatible with const-qualified type '" +
                               get_type_name(lhs_const->const_type) + "'.");
        }

        return err;
    } else if (rhs_const) {
        error::Error *err = nullptr;
        if (context.is_const) {
            // All types in a const assignment context are implicitly const, so this is allowed.
            err = require_assignable(lhs, rhs_const->const_type, node, message, context);
        } else if (is_const_strict(lhs)) {
            // Strict types are not safe to treat as non-const due to shallow copying semantics.
            err = &errors->E_T_ASGN_NOCNST(node, message, get_type_name(lhs), get_type_name(rhs));
            err->add_supplement(error::SupplementKind::Note,
                                "type '" + get_type_name(rhs) + "' is const-strict");
            err->add_supplement(error::SupplementKind::Help,
                                "consider using a const pointer, or ensure both sides are equally "
                                "const-qualified");

            return err;
        } else if (!context.is_copy) {
            // A const value cannot be treated as a non-const value with reference semantics.
            err = &errors->E_T_ASGN_NOCNST(node, message, get_type_name(lhs), get_type_name(rhs));
            err->add_supplement(error::SupplementKind::Help,
                                "try copying the underlying value, or ensure both sides are "
                                "equally const-qualified");

            return err;
        } else {
            err = require_assignable(lhs, rhs_const->const_type, node, message, context);
        }

        if (err != nullptr) {
            err->add_cause("Const type mismatch",
                           "Type '" + get_type_name(rhs_const->const_type) +
                               "' must be compatible with type '" + get_type_name(lhs) + "'.");
        }

        return err;
    }

    if (auto lhs_array = std::get_if<ArrayType>(&lhs_type.get_program_type())) {
        auto rhs_array = std::get_if<ArrayType>(&rhs_type.get_program_type());
        if (!rhs_array) {
            return &errors->E_T_ARR_NONARR(node, message, get_type_name(rhs));
        }

        if (rhs_array->size < lhs_array->size) {
            return &errors->E_T_ARR_SZ_MIS(node, message, lhs_array->size, rhs_array->size);
        }

        auto err = require_assignable(lhs_type.get_id(), rhs_type.get_id(), node, message, context);
        if (err != nullptr) {
            err->add_cause("Array element type mismatch",
                           "Array elements of " + get_type_name(lhs) + " must be compatible with " +
                               get_type_name(rhs));
            return err;
        }
    } else if (auto left_ptr = std::get_if<PointerType>(&lhs_type.get_program_type())) {
        auto lhs_pointee_id = left_ptr->pointee_type;
        auto right_ptr = std::get_if<PointerType>(&rhs_type.get_program_type());
        if (!right_ptr) {
            return &errors->E_T_PTR_NONPTR(node, message, get_type_name(rhs));
        }
        auto rhs_pointee_id = right_ptr->pointee_type;
        auto rhs_pointee = get_type(rhs_pointee_id);
        auto constraint = context.is_const || (!is_lifetime_strict(rhs_pointee_id) && context.is_copy)
                              ? LifetimeRelation::LessEqual
                              : LifetimeRelation::Equals;
        std::vector<error::Supplement> supplements;

        if (is_lifetime_strict(rhs_pointee_id) && !context.is_const) {
            supplements.push_back(
                error::Supplement{error::SupplementKind::Note,
                                  "pointee '" + get_type_name(rhs_pointee_id) +
                                      "' is lifetime-strict cannot have its lifetime shortened."});
            supplements.push_back(
                error::Supplement{error::SupplementKind::Help,
                                  "try using a const pointer to make this lifetime-permissive."});
        } else if (!context.is_copy) {
            supplements.push_back(
                error::Supplement{error::SupplementKind::Note,
                                  "constraint here is lifetime-strict, likely due to nested "
                                  "pointers, and cannot have its lifetime shortened."});
            supplements.push_back(
                error::Supplement{error::SupplementKind::Help,
                                  "try a const pointer to make this lifetime-permissive."});
        } else {
            supplements.push_back(
                error::Supplement{error::SupplementKind::Note,
                                  "pointee '" + get_type_name(rhs_pointee_id) +
                                      "' is lifetime-permissive, its lifetime may be shortened."});
        }

        std::string message = "'" + get_type_name(rhs) + "' assigned to pointer with lifetime '*" +
                              lifetimes->get_lifetime_by_id(left_ptr->lifetime)->get_debug_name() +
                              "'";

        lifetimes->add_constraint(left_ptr->lifetime,
                                  constraint,
                                  right_ptr->lifetime,
                                  ConstraintCause{message, node, std::move(supplements)});

        auto err = require_assignable(lhs_pointee_id,
                                      rhs_pointee_id,
                                      node,
                                      message,
                                      {.is_const = context.is_const, .is_copy = false});
        if (err != nullptr) {
            err->add_cause("Pointee type mismatch",
                           "Pointee type of " + get_type_name(lhs) +
                               " must be compatible with pointee type of " + get_type_name(rhs));
        }

        return err;
    }

    return &errors->E_T_ASGN_NOREL(node, message, get_type_name(lhs), get_type_name(rhs));
}

bool TypeOperations::types_equal(TypeId a, TypeId b) {
    auto left_type = get_type(a);
    auto right_type = get_type(b);
    if (left_type.is_error() || right_type.is_error()) {
        // Don't report spurious errors if one of the types is already an error
        return true;
    }

    return a == b;
}