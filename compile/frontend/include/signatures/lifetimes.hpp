#ifndef INCLUDE_SIGNATURES_LIFETIMES_HPP
#define INCLUDE_SIGNATURES_LIFETIMES_HPP

#include <cstddef>
#include <variant>
#include <string_view>
#include <unordered_map>
#include "ast/token.hpp"
#include "ast/node.hpp"
#include "ast/declarations.hpp"
#include "errors/errors.hpp"
#include "resolve/symbols.hpp"

namespace arena::sema {

    struct NamedLifetime {
        std::string_view name;
        const ast::PointerType *first_use;

        bool operator==(const NamedLifetime &other) const {
            return name == other.name && first_use == other.first_use;
        }
    };

    /**
     * Lifetimes such as `T *a`.
     */
    struct UnnamedPointerLifetime {
        const ast::PointerType *origin;

        bool operator==(const UnnamedPointerLifetime &other) const {
            return origin == other.origin;
        }
    };

    /**
     * The global lifetime, denoted by `static`.
     */
    struct GlobalLifetime {
        bool operator==(const GlobalLifetime &other) const {
            return true; // all instances of GlobalLifetime are considered equal
        }
    };

    /**
     * A lifetime corresponding to a stack frame, denoted by the block statement that introduces it.
     */
    struct StackLifetime {
        const ast::BlockStatement *block;
        bool operator==(const StackLifetime &other) const { return block == other.block; }
    };

    /**
     * A lifetime corresponding to an arena block, denoted by the arena statement that introduces
     * it.
     */
    struct ExplicitArenaLifetime {
        const ast::ArenaStatement *arena_block;
        bool operator==(const ExplicitArenaLifetime &other) const {
            return arena_block == other.arena_block;
        }
    };

    /**
     * The lifetime of a struct itself (e.g. `T *my`).
     */
    struct StructMyLifetime {
        bool operator==(const StructMyLifetime &other) const {
            return true; // all instances of StructMyLifetime are considered equal
        }
    };

    /**
     * The lifetime of a function's caller, denoted by `*ctx`.
     */
    struct FunctionContextLifetime {
        bool operator==(const FunctionContextLifetime &other) const {
            return true; // all instances of FunctionContextLifetime are considered equal
        }
    };

    /**
     * The `*any` lifetime.
     */
    struct AnyLifetime {
        bool operator==(const AnyLifetime &other) const {
            return true; // all instances of AnyLifetime are considered equal
        }
    };

    /**
     * The `*unsafe` lifetime, which disables lifetime checks.
     */
    struct UnsafeLifetime {
        bool operator==(const UnsafeLifetime &other) const {
            return true; // all instances of UnsafeLifetime are considered equal
        }
    };

    /**
     * A free lifetime corresponding to a locally declared pointer type such as `let x : T*` or
     * `x.as(T *)`, with no name.
     */
    struct FreeLifetime {
        const ast::PointerType *origin;

        bool operator==(const FreeLifetime &other) const { return origin == other.origin; }
    };

    using LifetimeVariant = std::variant<NamedLifetime,
                                         UnnamedPointerLifetime,
                                         GlobalLifetime,
                                         StackLifetime,
                                         ExplicitArenaLifetime,
                                         StructMyLifetime,
                                         FunctionContextLifetime,
                                         AnyLifetime,
                                         UnsafeLifetime,
                                         FreeLifetime>;

    enum class LifetimeRelation {
        Greater,
        GreaterEqual,
        Equals,
        LessEqual,
        Less,
    };

    struct LifetimeConstraint {
        LifetimeId left_id;
        LifetimeRelation relation;
        LifetimeId right_id;
        std::optional<error::Cause> origin;

        bool operator==(const LifetimeConstraint &other) const {
            return left_id.lt_id == other.left_id.lt_id && relation == other.relation &&
                   right_id.lt_id == other.right_id.lt_id;
        }
    };

    struct Lifetime {
        LifetimeVariant variant;
        LifetimeId group_lifetime_id;
        std::vector<std::pair<LifetimeId, LifetimeConstraint>> outlives;
        std::vector<std::pair<LifetimeId, LifetimeConstraint>> outlived_by;
        std::vector<std::pair<LifetimeId, LifetimeConstraint>> equals;

        std::optional<std::string_view> get_name() const;
        std::string get_debug_name() const;

        bool operator==(const Lifetime &other) const {
            return variant == other.variant &&
                   group_lifetime_id.lt_id == other.group_lifetime_id.lt_id;
        }
    };

    class LifetimeGroup {
    public:
        LifetimeGroup();

        Lifetime &add_lifetime(LifetimeVariant variant);
        std::optional<std::reference_wrapper<Lifetime>> get_lifetime_by_name(std::string_view name);
        const Lifetime *get_lifetime_by_id(LifetimeId id) const {
            if (id.lt_id < lifetimes.size()) {
                return &lifetimes[id.lt_id];
            }
            return nullptr;
        }
        void add_constraint(LifetimeConstraint constraint);
        void add_constraint(Lifetime &left,
                            LifetimeRelation relation,
                            Lifetime &right,
                            const error::Cause &origin);
        void add_constraint(LifetimeId left,
                            LifetimeRelation relation,
                            LifetimeId right,
                            const error::Cause &origin);
        void set_context(FunctionId function_id) { context = function_id; }
        void set_context(TypeId type_id) { context = type_id; }
        LifetimeId get_max_lifetime_id() const { return lifetimes.back().group_lifetime_id; }

        LifetimeId get_global_lifetime() const { return global_lifetime_id; }
        LifetimeId get_unsafe_lifetime() const { return unsafe_lifetime_id; }
        LifetimeId get_any_lifetime() const { return any_lifetime_id; }
        LifetimeId get_my_lifetime() const { return my_lifetime_id; }
        LifetimeId get_ctx_lifetime() const { return ctx_lifetime_id; }
        LifetimeId get_stack_root_lifetime() const { return stack_root_lifetime_id; }

        std::unordered_map<LifetimeId, LifetimeId> import(const LifetimeGroup &other);

        std::string to_string() const;
        bool operator==(const LifetimeGroup &other) const {
            return context == other.context && lifetimes == other.lifetimes &&
                   constraints == other.constraints;
        }

        const std::vector<LifetimeConstraint> &get_constraints() const { return constraints; }

    private:
        LifetimeId global_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        LifetimeId unsafe_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        LifetimeId any_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        LifetimeId my_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        LifetimeId ctx_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        LifetimeId stack_root_lifetime_id = LifetimeId{static_cast<size_t>(-1)};
        std::variant<TypeId, FunctionId> context;
        std::vector<Lifetime> lifetimes;
        std::vector<LifetimeConstraint> constraints;
        std::unordered_map<std::string_view, LifetimeId> lifetimes_by_name;
    };

    class LifetimeTable {
    public:
        LifetimeTable(LifetimeGroup *group, bool is_public);

        LifetimeId lookup(std::string_view name, const ast::PointerType *type) const;

        LifetimeId infer_lifetime(const ast::PointerType *type);

        LifetimeId get_arena_lifetime() const;
        LifetimeId get_stack_lifetime() const;

        class ArenaLifetimeGuard {
        public:
            ArenaLifetimeGuard(LifetimeTable *table, LifetimeId old_id)
                : table(table), old_id(old_id) {}

            ArenaLifetimeGuard(const ArenaLifetimeGuard &) = delete;
            ArenaLifetimeGuard(ArenaLifetimeGuard &&) = delete;
            ArenaLifetimeGuard &operator=(const ArenaLifetimeGuard &) = delete;
            ArenaLifetimeGuard &operator=(ArenaLifetimeGuard &&) = delete;

            ~ArenaLifetimeGuard() { table->current_arena_id = old_id; }

        private:
            LifetimeTable *table;
            LifetimeId old_id;
        };

        [[nodiscard]]
        ArenaLifetimeGuard push_arena(const ast::ArenaStatement *arena_stmt);

        class StackLifetimeGuard {
        public:
            StackLifetimeGuard(LifetimeTable *table, LifetimeId old_id)
                : table(table), old_id(old_id) {}

            StackLifetimeGuard(const StackLifetimeGuard &) = delete;
            StackLifetimeGuard(StackLifetimeGuard &&) = delete;
            StackLifetimeGuard &operator=(const StackLifetimeGuard &) = delete;
            StackLifetimeGuard &operator=(StackLifetimeGuard &&) = delete;

            ~StackLifetimeGuard() { table->current_stack_id = old_id; }

        private:
            LifetimeTable *table;
            LifetimeId old_id;
        };

        [[nodiscard]]
        StackLifetimeGuard push_stack(const ast::BlockStatement *block_stmt);

    private:
        LifetimeGroup *group;
        LifetimeId current_arena_id;
        LifetimeId current_stack_id;
        bool is_public;
        std::optional<LifetimeId> public_inferred_id = std::nullopt;
    };
} // namespace arena::sema

#endif // INCLUDE_SIGNATURES_LIFETIMES_HPP