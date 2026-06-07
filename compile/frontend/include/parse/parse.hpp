#ifndef INCLUDE_PARSE_PARSE_HPP
#define INCLUDE_PARSE_PARSE_HPP

#include "ast/declarations.hpp"
#include "errors/errors.hpp"
#include "util/arena.hpp"

namespace arena::parse {
    struct ParseResult {
        util::Arena ast_arena;
        std::vector<ast::Declaration *> declarations;
        std::vector<error::Error> errors;

        bool operator==(const ParseResult &other) const {
            // TODO: implement a proper equality check.
            return this == &other;
        }
    };

    ParseResult parse(std::string_view input);
} // namespace arena::parse

#endif