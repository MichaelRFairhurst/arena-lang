#include "signatures/lifetimes.hpp"
#include "ast/recursive_visitor.hpp"

using namespace arena::sema;
using namespace arena;

namespace {
    error::Supplement make_static_lifetime_note() {
        return {
            .kind = error::SupplementKind::Note,
            .message = "'*static' lifetimes exist for the entire "
                       "duration of the program",
        };
    }

    error::Supplement make_any_lifetime_note() {
        return {
            .kind = error::SupplementKind::Note,
            .message = "'*any' refers to an arbitrarily short lifetime",
        };
    }

    error::Supplement make_any_lifetime_help() {
        return {
            .kind = error::SupplementKind::Help,
            .message = "did you mean to use '*unsafe'?",
        };
    }

    error::Supplement make_my_lifetime_note() {
        return {
            .kind = error::SupplementKind::Note,
            .message = "the '*my' lifetime refers to the lifetime of an arbitrary struct object",
        };
    }

    error::Supplement make_outer_arena_lifetime_note() {
        return {
            .kind = error::SupplementKind::Note,
            .message = "the outer '*arena' lifetime matches the lifetime of the "
                       "caller's current arena.",
        };
    }

    LifetimeConstraint fixed_constraint(LifetimeId greater_lifetime_id,
                                        std::string greater_lifetime_name,
                                        LifetimeId lesser_lifetime_id,
                                        std::string lesser_lifetime_name,
                                        LifetimeError error,
                                        std::vector<error::Supplement> supplements) {
        return {
            greater_lifetime_id,
            LifetimeRelation::Greater,
            lesser_lifetime_id,
            ConstraintCause{
                .description = "'*" + greater_lifetime_name + "' lifetime is greater than '*" +
                               lesser_lifetime_name + "'",
                .supplements = supplements,
                .error = error,
            },
        };
    }
} // namespace

std::optional<std::string_view> Lifetime::get_name() const {
    if (auto named_lifetime = std::get_if<NamedLifetime>(&variant)) {
        return named_lifetime->name;
    } else if (std::holds_alternative<GlobalLifetime>(variant)) {
        return "static";
    } else if (std::holds_alternative<StructMyLifetime>(variant)) {
        return "my";
    } else if (std::holds_alternative<AnyLifetime>(variant)) {
        return "any";
    } else if (std::holds_alternative<UnsafeLifetime>(variant)) {
        return "unsafe";
    }

    return std::nullopt;
}

std::string Lifetime::get_debug_name() const {
    auto name = get_name();
    if (name) {
        return std::string(*name);
    } else if (std::holds_alternative<StackLifetime>(variant)) {
        return "stack";
    } else if (std::holds_alternative<FunctionContextLifetime>(variant)) {
        return "arena<ctx>";
    } else if (std::holds_alternative<ExplicitArenaLifetime>(variant)) {
        return "arena";
    } else if (std::holds_alternative<UnnamedPointerLifetime>(variant)) {
        return "<unspecified>";
    } else if (std::holds_alternative<FreeLifetime>(variant)) {
        return "<free>";
    }

    throw std::runtime_error("Unknown lifetime variant");
}

LifetimeGroup::LifetimeGroup() {
    global_lifetime_id = add_lifetime(GlobalLifetime{}).group_lifetime_id;
    unsafe_lifetime_id = add_lifetime(UnsafeLifetime{}).group_lifetime_id;
    any_lifetime_id = add_lifetime(AnyLifetime{}).group_lifetime_id;
    my_lifetime_id = add_lifetime(StructMyLifetime{}).group_lifetime_id;
    ctx_lifetime_id = add_lifetime(FunctionContextLifetime{}).group_lifetime_id;

    add_constraint(fixed_constraint(global_lifetime_id,
                                    "static",
                                    any_lifetime_id,
                                    "any",
                                    LifetimeError::InvalidUseOfAny,
                                    {make_static_lifetime_note(),
                                     make_any_lifetime_note(),
                                     make_any_lifetime_help()}));

    add_constraint(fixed_constraint(global_lifetime_id,
                                    "static",
                                    my_lifetime_id,
                                    "my",
                                    LifetimeError::InvalidUseOfMy,
                                    {make_static_lifetime_note(), make_my_lifetime_note()}));

    add_constraint(
        fixed_constraint(global_lifetime_id,
                         "static",
                         ctx_lifetime_id,
                         "arena<ctx>",
                         LifetimeError::EscapesContext,
                         {make_static_lifetime_note(), make_outer_arena_lifetime_note()}));

    add_constraint(fixed_constraint(ctx_lifetime_id,
                                    "arena<ctx>",
                                    any_lifetime_id,
                                    "any",
                                    LifetimeError::InvalidUseOfAny,
                                    {make_outer_arena_lifetime_note(),
                                     make_any_lifetime_note(),
                                     make_any_lifetime_help()}));
}

Lifetime &LifetimeGroup::add_lifetime(LifetimeVariant variant) {
    LifetimeId id{lifetimes.size()};
    lifetimes.push_back(Lifetime{variant, id});
    auto &result = lifetimes.back();
    if (auto name = result.get_name()) {
        lifetimes_by_name[*name] = id;
    }

    if (stack_root_lifetime_id.lt_id == static_cast<size_t>(-1) &&
        std::holds_alternative<StackLifetime>(variant)) {
        stack_root_lifetime_id = id;
    }

    return result;
}

std::optional<std::reference_wrapper<Lifetime>> LifetimeGroup::get_lifetime_by_name(
    std::string_view name) {
    if (auto it = lifetimes_by_name.find(name); it != lifetimes_by_name.end()) {
        return lifetimes[it->second.lt_id];
    }
    return std::nullopt;
}

void LifetimeGroup::add_constraint(LifetimeConstraint constraint) {
    constraints.push_back(constraint);
    auto &left = lifetimes[constraint.left_id.lt_id];
    auto &right = lifetimes[constraint.right_id.lt_id];
    switch (constraint.relation) {
    case LifetimeRelation::Greater:
    case LifetimeRelation::GreaterEqual:
        left.outlives.emplace_back(right.group_lifetime_id, constraint);
        right.outlived_by.emplace_back(left.group_lifetime_id, constraint);
        break;
    case LifetimeRelation::Equals:
        left.equals.emplace_back(right.group_lifetime_id, constraint);
        right.equals.emplace_back(left.group_lifetime_id, constraint);
        break;
    case LifetimeRelation::Less:
    case LifetimeRelation::LessEqual:
        left.outlived_by.emplace_back(right.group_lifetime_id, constraint);
        right.outlives.emplace_back(left.group_lifetime_id, constraint);
        break;
    }
}

void LifetimeGroup::add_constraint(Lifetime &left,
                                   LifetimeRelation relation,
                                   Lifetime &right,
                                   const ConstraintCause &origin) {
    add_constraint(left.group_lifetime_id, relation, right.group_lifetime_id, origin);
}

void LifetimeGroup::add_constraint(LifetimeId left,
                                   LifetimeRelation relation,
                                   LifetimeId right,
                                   const ConstraintCause &origin) {
    add_constraint(LifetimeConstraint{left, relation, right, origin});
}

class LifetimeImportVisitor {
public:
    LifetimeImportVisitor(LifetimeGroup *importing_group) : importing_group(importing_group) {}

    LifetimeId operator()(const GlobalLifetime &global_lifetime) {
        return importing_group->get_global_lifetime();
    }

    LifetimeId operator()(const AnyLifetime &any_lifetime) {
        return importing_group->get_any_lifetime();
    }

    LifetimeId operator()(const UnsafeLifetime &unsafe_lifetime) {
        return importing_group->get_unsafe_lifetime();
    }

    LifetimeId operator()(const StructMyLifetime &my_lifetime) {
        // TODO: Handle struct lifetimes properly.
        return importing_group->get_my_lifetime();
    }

    LifetimeId operator()(const FunctionContextLifetime &function_context_lifetime) {
        // TODO: Handle function context lifetimes properly.
        return importing_group->get_ctx_lifetime();
    }

    LifetimeId operator()(const StackLifetime &stack_lifetime) {
        throw std::runtime_error("Imported group should not contain stack lifetimes");
    }

    LifetimeId operator()(const ExplicitArenaLifetime &arena_lifetime) {
        throw std::runtime_error("Imported group should not contain explicit arena lifetimes");
    }

    LifetimeId operator()(const FreeLifetime &free_lifetime) {
        return importing_group->add_lifetime(free_lifetime).group_lifetime_id;
    }

    LifetimeId operator()(const NamedLifetime &named_lifetime) {
        return importing_group->add_lifetime(named_lifetime).group_lifetime_id;
    }

    LifetimeId operator()(const UnnamedPointerLifetime &unnamed_pointer_lifetime) {
        return importing_group->add_lifetime(unnamed_pointer_lifetime).group_lifetime_id;
    }

private:
    LifetimeGroup *importing_group;
};

std::unordered_map<LifetimeId, LifetimeId> LifetimeGroup::import(const LifetimeGroup &other) {
    std::unordered_map<LifetimeId, LifetimeId> id_mapping;
    for (const auto &lifetime : other.lifetimes) {
        auto imported_id = std::visit(LifetimeImportVisitor{this}, lifetime.variant);
        id_mapping[lifetime.group_lifetime_id] = imported_id;
    }

    for (const auto &constraint : other.constraints) {
        auto left_it = id_mapping.find(constraint.left_id);
        auto right_it = id_mapping.find(constraint.right_id);
        if (left_it != id_mapping.end() && right_it != id_mapping.end()) {
            add_constraint(LifetimeConstraint{left_it->second,
                                              constraint.relation,
                                              right_it->second,
                                              constraint.origin});
        }
    }

    return id_mapping;
}

std::string LifetimeGroup::to_string() const {
    std::string result = "Lifetimes:\n";
    for (const auto &lifetime : lifetimes) {
        result += "  " + std::to_string(lifetime.group_lifetime_id.lt_id) + ": " +
                  lifetime.get_debug_name() + "\n";
    }
    result += "Constraints:\n";
    for (const auto &constraint : constraints) {
        auto left = lifetimes[constraint.left_id.lt_id];
        auto right = lifetimes[constraint.right_id.lt_id];
        auto left_name =
            "L" + std::to_string(constraint.left_id.lt_id) + ": " + left.get_debug_name();
        auto right_name =
            "L" + std::to_string(constraint.right_id.lt_id) + ": " + right.get_debug_name();
        result += "  " + left_name + " ";
        switch (constraint.relation) {
        case LifetimeRelation::Greater:
            result += ">";
            break;
        case LifetimeRelation::GreaterEqual:
            result += ">=";
            break;
        case LifetimeRelation::Equals:
            result += "=";
            break;
        case LifetimeRelation::LessEqual:
            result += "<=";
            break;
        case LifetimeRelation::Less:
            result += "<";
            break;
        }
        result += " " + right_name;
        if (constraint.origin.has_value()) {
            result += " (origin: " + std::string(constraint.origin->description) + " at " +
                      std::string(constraint.origin->location->begin->text) + "..." +
                      std::string(constraint.origin->location->end->text) + ")";
        }
        result += "\n";
    }
    return result;
}

LifetimeTable::LifetimeTable(LifetimeGroup *group, bool is_public)
    : group(group), current_arena_id(group->get_ctx_lifetime()),
      current_stack_id(group->get_stack_root_lifetime()), is_public(is_public) {
    current_arena_id = group->get_ctx_lifetime();
    current_stack_id = group->get_stack_root_lifetime();
}

LifetimeId LifetimeTable::lookup(std::string_view name, const ast::PointerType *type) const {
    if (name == "arena") {
        return current_arena_id;
    } else if (name == "stack") {
        return current_stack_id;
    } else if (auto it = group->get_lifetime_by_name(name); it.has_value()) {
        return it->get().group_lifetime_id;
    } else {
        return group->add_lifetime(NamedLifetime{name, type}).group_lifetime_id;
    }
}

LifetimeId LifetimeTable::infer_lifetime(const ast::PointerType *type) {
    if (is_public) {
        if (!public_inferred_id.has_value()) {
            public_inferred_id = group->add_lifetime(FreeLifetime{type}).group_lifetime_id;
        }
        return *public_inferred_id;
    } else {
        return group->add_lifetime(FreeLifetime{type}).group_lifetime_id;
    }
}

LifetimeId LifetimeTable::get_arena_lifetime() const { return current_arena_id; }
LifetimeId LifetimeTable::get_stack_lifetime() const { return current_stack_id; }

[[nodiscard]]
LifetimeTable::ArenaLifetimeGuard LifetimeTable::push_arena(const ast::ArenaStatement *arena_stmt) {
    auto old_id = current_arena_id;
    current_arena_id = group->add_lifetime(ExplicitArenaLifetime{arena_stmt}).group_lifetime_id;

    group->add_constraint(fixed_constraint(old_id,
                                           "arena<outer>",
                                           current_arena_id,
                                           "arena<inner>",
                                           LifetimeError::EscapesArena,
                                           {{.kind = error::SupplementKind::Note,
                                             .location = arena_stmt,
                                             .message = "'*arena<inner>' defined here"}}));

    group->add_constraint(fixed_constraint(current_arena_id,
                                           "arena",
                                           group->get_any_lifetime(),
                                           "any",
                                           LifetimeError::InvalidUseOfAny,
                                           {make_any_lifetime_note(), make_any_lifetime_help()}));

    return ArenaLifetimeGuard(this, old_id);
}

[[nodiscard]]
LifetimeTable::StackLifetimeGuard LifetimeTable::push_stack(const ast::BlockStatement *block_stmt) {
    auto old_id = current_stack_id;
    current_stack_id = group->add_lifetime(StackLifetime{block_stmt}).group_lifetime_id;

    error::Supplement stack_note;

    if (old_id.lt_id == static_cast<size_t>(-1)) {
        stack_note = {.kind = error::SupplementKind::Note,
                      .location = block_stmt,
                      .message = "involving '*stack' lifetime for this function block"};
        group->add_constraint(fixed_constraint(group->get_ctx_lifetime(),
                                               "arena<ctx>",
                                               current_stack_id,
                                               "stack",
                                               LifetimeError::EscapesStack,
                                               {stack_note}));
    } else {
        stack_note = {.kind = error::SupplementKind::Note,
                      .location = block_stmt,
                      .message = "involving '*stack' lifetime for this inner block"};

        group->add_constraint(fixed_constraint(old_id,
                                               "stack<outer>",
                                               current_stack_id,
                                               "stack<inner>",
                                               LifetimeError::EscapesBlock,
                                               {stack_note}));
    }

    group->add_constraint(fixed_constraint(group->get_global_lifetime(),
                                           "static",
                                           current_stack_id,
                                           "stack",
                                           LifetimeError::EscapesStack,
                                           {make_static_lifetime_note(), stack_note}));

    group->add_constraint(
        fixed_constraint(current_stack_id,
                         "*stack",
                         group->get_any_lifetime(),
                         "*any",
                         LifetimeError::InvalidUseOfAny,
                         {make_any_lifetime_note(), make_any_lifetime_help(), stack_note}));

    return StackLifetimeGuard(this, old_id);
}