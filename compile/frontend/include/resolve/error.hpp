#ifndef INCLUDE_RESOLVE_ERROR_HPP
#define INCLUDE_RESOLVE_ERROR_HPP

#include <string>
#include "ast/node.hpp"

namespace arena::sema {

    struct ResolveError {
        std::string message;
        const ast::Node *node;

        ResolveError(std::string message, const ast::Node *node) : message(std::move(message)), node(node) {}

        bool operator==(const ResolveError &other) const {
            return message == other.message && node == other.node;
        }

        bool operator!=(const ResolveError &other) const {
            return !(*this == other);
        }
    };

} // namespace arena::sema

#endif