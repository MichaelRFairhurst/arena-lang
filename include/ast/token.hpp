#ifndef ARENA_INCLUDE_AST_TOKEN_HPP
#define ARENA_INCLUDE_AST_TOKEN_HPP

#include <string_view>

namespace arena::ast {

enum class TokenType
{
    Identifier,
    Literal,
    OPEN_BRACE,
    CLOSE_BRACE,
    OPEN_PAREN,
    CLOSE_PAREN,
    SEMICOLON,
    COMMA,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    EQUAL,
    EQUAL_EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,
    AND,
    OR
};

struct Token
{
    TokenType type;
    std::string_view text;
};

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_TOKEN_HPP