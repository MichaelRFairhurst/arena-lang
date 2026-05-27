#ifndef INCLUDE_PARSE_PARSE_HPP
#define INCLUDE_PARSE_PARSE_HPP

#include "ast/declarations.hpp"
extern "C" {
#include "arena.h"
}

namespace arena::parse {
    struct ParseError {
        std::string_view message;
        size_t position;
    };

    struct ParseResult {
        rena_arena ast_arena;
        std::vector<ast::Declaration *> declarations;
        std::vector<ParseError> errors;

        ParseResult() {
            ast_arena.head = nullptr;
        }

        ParseResult(const ParseResult &) = delete;
        ParseResult &operator=(const ParseResult &) = delete;
        ParseResult(ParseResult &&other) {
            // Move the arena and declarations, but leave the old arena in a valid state.
            ast_arena = std::move(other.ast_arena);
            declarations = std::move(other.declarations);
            errors = std::move(other.errors);
            other.ast_arena.head = nullptr;
        }
        ParseResult &operator=(ParseResult &&other) {
            if (this != &other) {
                rena_arena_free(&ast_arena);

                // Move the arena and declarations, but leave the old arena in a valid state.
                ast_arena = std::move(other.ast_arena);
                declarations = std::move(other.declarations);
                errors = std::move(other.errors);
                other.ast_arena.head = nullptr;
            }
            return *this;
        }

        ~ParseResult() {
            rena_arena_free(&ast_arena);
        }

        bool operator==(const ParseResult &other) const {
            // TODO: implement a proper equality check.
            return this == &other;
        }
    };

    ParseResult parse(std::string_view input);
}

#endif