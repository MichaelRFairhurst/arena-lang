#ifndef ARENA_INCLUDE_ERRORS_ERRORS_HPP
#define ARENA_INCLUDE_ERRORS_ERRORS_HPP

#include <string>
#include <variant>
#include <vector>
#include "ast/node.hpp"

namespace arena::error {

    struct Link {
        Link() = default;
        Link(const ast::Node *origin)
            : message("here"), begin(origin->begin()), end(origin->end()) {}
        Link(const ast::Node *origin, std::string message)
            : message(std::move(message)), begin(origin->begin()), end(origin->end()) {}
        Link(const ast::Token *begin, const ast::Token *end, std::string message)
            : message(std::move(message)), begin(begin), end(end) {}

        std::string message;
        const ast::Token *begin;
        const ast::Token *end;

        bool operator==(const Link &other) const {
            return message == other.message && begin == other.begin && end == other.end;
        }
    };

    using Chunk = std::variant<std::string, Link>;

    class Error {
    public:
        Error(const ast::Token *begin, const ast::Token *end, std::string message)
            : chunks{std::move(message)}, begin(begin), end(end) {}
        Error(const ast::Node *node, std::string message)
            : chunks{std::move(message)}, begin(node->begin()), end(node->end()) {}

        template <typename... Args>
        Error(const ast::Token *begin, const ast::Token *end, Args &&...args)
            : chunks{std::forward<Args>(args)...}, begin(begin), end(end) {}

        template <typename... Args>
        Error(const ast::Node *node, Args &&...args)
            : chunks{std::forward<Args>(args)...}, begin(node->begin()), end(node->end()) {}

        std::string to_string() const {
            std::string result = "At " + range_string(begin, end) + ": ";
            for (const auto &chunk : chunks) {
                if (auto str = std::get_if<std::string>(&chunk)) {
                    result += *str;
                } else if (auto link = std::get_if<Link>(&chunk)) {
                    result += "[" + link->message + "](at '" +
                              range_string(link->begin, link->end) + "')";
                }
            }
            return result;
        }

        bool operator==(const Error &other) const {
            return chunks == other.chunks && begin == other.begin && end == other.end;
        }

        bool operator!=(const Error &other) const { return !(*this == other); }

    private:
        std::string range_string(const ast::Token *start, const ast::Token *end) const {
            if (start == end) {
                return std::string(start->text);
            }

            return std::string(start->text) + "..." + std::string(end->text);
        }

        std::vector<Chunk> chunks;
        const ast::Token *begin;
        const ast::Token *end;
    };

    class Reporter {
    public:
        template <typename... Args>
        void report(Args &&...args) {
            errors.emplace_back(std::forward<Args>(args)...);
        }

        const std::vector<Error> &get_errors() const { return errors; }

    private:
        std::vector<Error> errors;
    };

} // namespace arena::error

#endif