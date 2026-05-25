#ifndef ARENA_INCLUDE_AST_TOKEN_HPP
#define ARENA_INCLUDE_AST_TOKEN_HPP

#include <string_view>

namespace arena::ast {

    enum class TokenType {
        IDENTIFIER,
        INTEGER,
        STRING,
        FLOATING,
        TRUE,
        FALSE,
        OPEN_BRACE,
        CLOSE_BRACE,
        OPEN_PAREN,
        CLOSE_PAREN,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        SEMICOLON,
        COLON,
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
        OR,
        NOT,
        DOT,
        AMP,
        ARROW,
        AS,
        FUN,
        IF,
        ELSE,
        LET,
        WHILE,
        FOR,
        RET,
        IMPORT,
        STRUCT,
        UNINITIALIZED_TOKEN,
        END_OF_INPUT,
    };

    struct Token {
        TokenType type = TokenType::UNINITIALIZED_TOKEN;
        std::string_view text;
        Token *next = nullptr;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_TOKEN_HPP