#ifndef ARENA_INCLUDE_AST_LITERALS_HPP
#define ARENA_INCLUDE_AST_LITERALS_HPP

#include "ast/node.hpp"

namespace arena::ast {
    class Literal : public Node {
    public:
        Literal(Token *literal) : Node(literal, literal) {}

        virtual ~Literal() = default;

        std::string to_string() const {
            return std::string(begin()->text);
        }
    };

    class StringLiteral : public Literal {
    public:
        StringLiteral(Token *token) : Literal(token) {}
    };

    class IntegerLiteral : public Literal {
    public:
        IntegerLiteral(Token *token) : Literal(token) {}
    };
} // namespace arena::ast
#endif // ARENA_INCLUDE_AST_LITERALS_HPP