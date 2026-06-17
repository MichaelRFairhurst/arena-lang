#ifndef ARENA_INCLUDE_ERRORS_ERRORS_HPP
#define ARENA_INCLUDE_ERRORS_ERRORS_HPP

#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "ast/node.hpp"

/**
 * Arena errors have the following general structure:
 *
 * - Error code
 *   - E, W, or N for error, warning, or note
 *   - A letter code for the category (P for parsing, T for typechecking, L for lifetime)
 *   - A short code for the specific error (e.g., UNEXP for unexpected token)
 * - Location (start and end token, may be the same)
 * - A human-readable description of the selected code (e.g., "unexpected token")
 * - A list of "chunks" that together form the error message. Each chunk is either:
 *   - A string (e.g., "Expected a type but found ")
 *   - A hyperlink (in the IDE) with
 *     - A source location (start and end token)
 *     - Link text (e.g., "int")
 *     - Label text used in the CLI to describe the selected code (e.g., "found type is")
 * - Optional supplementary notes
 *   - A note kind (e.g., "Note" or "Help")
 *   - A human-readable description of the note
 *   - An optional source location (start and end token)
 * - A list of causes, where each cause has
 *   - Summary (e.g., "a < b", "b < c", "c < a")
 *   - Full description ("assignment requires that ....")
 *   - Source location (start and end token)
 *   - A list of supplementary notes
 *
 * Overall, on the CLI will be rendered something like:
 *
 * ```
 * Error[E-P-MIS]: Stack lifetime variable escapes its scope
 * --> src/main.arena:5:10
 *  :      x = y.&;
 *  :          ^^^ assigned here
 *  :
 *  : Conflicting requirements:
 *  : 1. *stack lifetime must be greater than *static lifetime
 *  : 2. *static lifetime must be greater than *stack lifetime
 *  :
 * == Note: Variable has lifetime of block here src/main.arena:3:5
 * [[src/main.arena:3:5]]:
 *  :         v
 *  : fun f() {
 *  :    ... (3 more lines) ...
 *  : }
 *  : ^ variable lifetime is this block
 *  :
 * caused by 1. *stack lifetime must be greater than *static lifetime:
 * [[src/main.arena:3:5]]:
 *  :      let x: *static;
 *  :      ^^^^^^^^^^^^^^ use of static lifetime
 *  :
 * caused by 2. *static lifetime must be greater than *stack lifetime:
 * [[src/main.arena:2:5]]:
 *  :      x = y.&;
 *  :          ^^^ address of variable
 *  :
 * == Note: variable 'y' declared with stack lifetime here
 * [[src/main.arena:1:5]]:
 *  :      let y: i32;
 *  :      ^^^^^^^^^^ variable declaration
 *  :
 * == Note: Every lifetime is less than the static lifetime
 * == Help: Consider assigning by value instead of by reference
 * ```
 */
namespace arena::error {

    struct Location {
        Location() = default;
        Location(const ast::Token *begin, const ast::Token *end) : begin(begin), end(end) {}
        Location(const ast::Node *node) : begin(node->begin()), end(node->end()) {}
        Location(const ast::Token *token) : begin(token), end(token) {}

        const ast::Token *begin;
        const ast::Token *end;

        bool operator==(const Location &other) const {
            return begin == other.begin && end == other.end;
        }
    };

    enum class SupplementKind { Note, Help };

    struct Supplement {
        SupplementKind kind;
        std::string message;
        std::optional<Location> location;

        bool operator==(const Supplement &other) const {
            return kind == other.kind && message == other.message && location == other.location;
        }
    };

    struct Cause {
        std::string summary;
        std::string full_description;
        std::optional<Location> location;
        std::vector<Supplement> supplements;

        bool operator==(const Cause &other) const {
            return summary == other.summary && full_description == other.full_description &&
                   location == other.location && supplements == other.supplements;
        }
    };

    using LocatedText = std::pair<Location, std::string>;

    struct Link {
        std::string link_text;
        std::string label_text;
        Location location;

        bool operator==(const Link &other) const {
            return link_text == other.link_text && label_text == other.label_text &&
                   location == other.location;
        }
    };

    using Chunk = std::variant<std::string, std::string_view, const char *, Link>;

    class Error {
    public:
        template <typename... Args>
        Error(std::string_view code, Location location, std::string_view label, Args &&...args)
            : code(code), location(location), label(label), chunks{std::forward<Args>(args)...} {}

        bool operator==(const Error &other) const {
            return chunks == other.chunks && location == other.location;
        }

        bool operator!=(const Error &other) const { return !(*this == other); }

        std::string_view get_code() const { return code; }
        bool is_lifetime_error() const { return get_code().substr(0, 3) == "E-L"; }
        bool is_type_error() const { return get_code().substr(0, 3) == "E-T"; }

        const Location &get_location() const { return location; }
        std::string_view get_label() const { return label; }

        const std::vector<Chunk> &get_chunks() const { return chunks; }
        const std::vector<Supplement> &get_supplements() const { return supplements; }
        const std::vector<Cause> &get_causes() const { return causes; }

        const ast::Token *get_begin() const { return location.begin; }
        const ast::Token *get_end() const { return location.end; }

        void add_cause(std::string summary,
                       std::string full_description,
                       std::optional<Location> location = std::nullopt,
                       std::vector<Supplement> supplements = {}) {
            causes.push_back({summary, full_description, location, supplements});
        }

        void add_supplement(SupplementKind kind,
                            std::string message,
                            std::optional<Location> location = std::nullopt) {
            supplements.push_back({kind, message, location});
        }

    private:
        std::string range_string(const ast::Token *start, const ast::Token *end) const {
            if (start == end) {
                return std::string(start->text);
            }

            return std::string(start->text) + "..." + std::string(end->text);
        }

        std::string_view code;
        Location location;
        std::string label;
        std::vector<Chunk> chunks;
        std::vector<Supplement> supplements;
        std::vector<Cause> causes;
    };

    class Reporter {
    public:
        // Parse errors
        Error &E_P_UNEXP(Location l, std::string_view expected, std::string_view actual) {
            return report({"E-P_UNEXP",
                           l,
                           "Unexpected token",
                           std::vector<Chunk>{"Expected ", expected, " but found ", actual}});
        }

        // static constexpr ErrorCode E_P_MIS_RET = {"E-P_MIS_RET", "Expected a return type in
        // function declaration"};

        // Typechecking errors
        Error &E_T_BIN_MIS(Location l, LocatedText left, LocatedText right) {
            return report({
                "E-T_BIN_MIS",
                l,
                "In this binary expression",
                std::vector<Chunk>{
                    "Expected both binary operands to have matching types, but got ",
                    Link{.link_text = left.second,
                         .label_text = "left operand type is " + left.second,
                         .location = left.first},
                    " and ",
                    Link{.link_text = right.second,
                         .label_text = "right operand type is " + right.second,
                         .location = right.first},
                },
            });
        }

        Error &E_T_ARR_SZ_MIS(Location l,
                              std::string message,
                              size_t expected_size,
                              size_t actual_size) {
            return report({
                "E-T_ARR_SZ_MIS",
                l,
                "array size mismatch",
                std::vector<Chunk>{"Expected an array of size at least ",
                                   std::to_string(expected_size),
                                   ", but got ",
                                   std::to_string(actual_size),
                                   " in ",
                                   message},
            });
        }

        Error &E_T_ARR_NONARR(Location l, std::string message, std::string actual_type) {
            return report({
                "E-T_ARR_NONARR",
                l,
                "array element type mismatch",
                std::vector<Chunk>{"Expected a compatible array type, but got ",
                                   actual_type,
                                   " in ",
                                   message},
            });
        }

        Error &E_T_PTR_NONPTR(Location l, std::string message, std::string actual_type) {
            return report({
                "E-T_PTR_NONPTR",
                l,
                "pointer type mismatch",
                std::vector<Chunk>{"Expected a compatible pointer type, but got ",
                                   actual_type,
                                   " in ",
                                   message},
            });
        }

        Error &E_T_ASGN_NOREL(Location l,
                              std::string message,
                              std::string expected,
                              std::string actual_type) {
            return report({
                "E-T_ASGN_NOREL",
                l,
                "type mismatch",
                std::vector<
                    Chunk>{"Expected ", expected, " but got ", actual_type, " in ", message},
            });
        }

        Error &E_T_CANT_INFER_V(Location l, std::string_view variable_name) {
            return report({
                "E-T_CANT_INFER_V",
                l,
                "At this variable declaration",
                std::vector<Chunk>{"Cannot infer type for variable '", variable_name, "'"},
            });
        }

        Error &E_T_CANT_INFER_EX(Location l) {
            return report({
                "E-T_CANT_INFER_EX",
                l,
                "For this expression",
                std::vector<Chunk>{"Cannot infer type for expression here"},
            });
        }

        Error &E_T_RV_CANT_ADDR(Location l, const ast::Node *operand) {
            auto &err = report({
                "E-T_RV_CANT_ADDR",
                l,
                "Address-of expression '.&' here",
                std::vector<Chunk>{
                    "Cannot take address of ",
                    Link{.link_text = "rvalue expression",
                         .label_text = "expression is an rvalue",
                         .location = operand},
                },
            });

            err.add_supplement(SupplementKind::Note,
                               "Only 'lvalue' expressions (ones that can be assigned to) can have "
                               "their address taken");

            return err;
        }

        Error &E_T_CANT_DEREF(Location l, LocatedText operand) {
            return report({
                "E-T_CANT_DEREF",
                l,
                "Pointer dereference with '.*'",
                std::vector<Chunk>{
                    "Expected a dereferenceable type, but got ",
                    Link{.link_text = operand.second,
                         .label_text = "expression has type " + operand.second,
                         .location = operand.first},
                },
            });
        }

        Error &E_T_CALL_WRG_ARGC(Location l,
                                 std::string_view function_name,
                                 size_t expected,
                                 size_t actual) {
            return report({
                "E-T_CALL_WRG_ARGC",
                l,
                "In function call expression here",
                std::vector<Chunk>{
                    "Incorrect number of arguments in call to function '",
                    function_name,
                    "', expected ",
                    std::to_string(expected),
                    " but got ",
                    std::to_string(actual),
                },
            });
        }

        Error &E_R_UNKN_FUNC(Location l, Location target) {
            return report({
                "E-R_UNKN_FUNC",
                l,
                "In function call expression here",
                std::vector<Chunk>{
                    "Unknown ",
                    Link{
                        .link_text = "target function",
                        .label_text = "target of call here",
                        .location = target,
                    },
                    " in call expression",
                },
            });
        }

        Error &E_T_NOT_CALLABLE(Location l, Location callee) {
            return report({
                "E-T_NOT_CALLABLE",
                l,
                "In function call expression here",
                std::vector<Chunk>{
                    Link{
                        .link_text = "Target expression",
                        .label_text = "target of call here",
                        .location = callee,
                    },
                    " is not callable",
                },
            });
        }

        Error &E_T_ASGN_RV(Location l, Location lhs) {
            return report({
                "E-T_ASGN_RV",
                l,
                "In this assignment",
                std::vector<Chunk>{
                    Link{
                        .link_text = "Left hand side",
                        .label_text = "not an lvalue expression",
                        .location = lhs,
                    },
                    " is not assignable because it is not an lvalue expression",
                },
            });
        }

        Error &E_L_ESC_STACK(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_ESC_STACK",
                l,
                "Escape occurs here",
                std::vector<Chunk>{
                    "Pointer value with stack lifetime escapes the function stack",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_ESC_BLOCK(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_ESC_BLOCK",
                l,
                "Escape occurs here",
                std::vector<Chunk>{
                    "Pointer value to stack escapes the lifetime of its block",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_ESC_CTX(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_ESC_CTX",
                l,
                "Escape occurs here",
                std::vector<Chunk>{
                    "Pointer value to caller's arena escapes that arena",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_ESC_RNA(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_ESC_RNA",
                l,
                "Escape occurs here",
                std::vector<Chunk>{
                    "Pointer value to a nested arena escapes its arena block",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_INV_ANY(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_INV_ANY",
                l,
                "Results in invalid use of '*any' lifetime",
                std::vector<Chunk>{
                    "Pointer to '*any' lifetime is used in a way that requires a more specific "
                    "lifetime",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_INV_MY(Location l, std::vector<Cause> causes) {
            auto &err = report({
                "E-L_INV_MY",
                l,
                "Results in invalid use of '*my' lifetime",
                std::vector<Chunk>{
                    "The '*my' lifetime does not guarantee a lifetime that can be used in this way",
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_L_OUTLV_VIOL(Location l,
                              std::string ltshort,
                              std::string ltlong,
                              std::vector<Cause> causes) {
            auto &err = report({
                "E-L_OUTLV_VIOL",
                l,
                causes.front().full_description,
                std::vector<Chunk>{
                    "Lifetime violation between *",
                    ltshort,
                    " and *",
                    ltlong,
                },
            });

            for (const auto &cause : causes) {
                err.add_cause(cause.summary,
                              cause.full_description,
                              cause.location,
                              cause.supplements);
            }
            return err;
        }

        Error &E_R_VAR_AMBG(Location l, std::string_view id) {
            return report({
                "E-R_VAR_AMBG",
                l,
                "For this identifier expression",
                std::vector<Chunk>{"Identifier ",
                                   id,
                                   " is ambiguous as it could refer to a function or a variable"},
            });
        }

        Error &E_R_ID_UNKN(Location l, std::string_view id) {
            return report({
                "E-R_ID_UNKN",
                l,
                "For this identifier expression",
                std::vector<Chunk>{
                    "Unresolved identifier ",
                    id,
                },
            });
        }

        template <typename... Args>
        Error &report(Args &&...args) {
            return errors.emplace_back(std::forward<Args>(args)...);
        }

        Error &report(Error error) {
            errors.push_back(std::move(error));
            return errors.back();
        }

        const std::vector<Error> &get_errors() const { return errors; }

    private:
        std::vector<Error> errors;
    };

} // namespace arena::error

template<>
struct std::hash<arena::error::Location> {
    size_t operator()(const arena::error::Location &loc) const {
        return std::hash<const char*>()(loc.begin->text.data()) ^
               (std::hash<const char*>()(loc.end->text.data()) << 1);
    }
};
#endif