#ifndef ARENA_INCLUDE_AST_LITERALS_HPP
#define ARENA_INCLUDE_AST_LITERALS_HPP

#include <string>
#include "ast/node.hpp"
#include "ast/visitor.hpp"

namespace arena::ast {
    class Literal : public Node {
    public:
        Literal(Token *literal) : Node(literal, literal) {}

        virtual ~Literal() = default;

        std::string to_string() const {
            return std::string(begin()->text);
        }
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class StringLiteral : public Literal {
    public:
        StringLiteral(Token *token) : Literal(token) {}
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class IntegerLiteral : public Literal {
    public:
        IntegerLiteral(Token *token) : Literal(token) {}
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };
} // namespace arena::ast
#endif // ARENA_INCLUDE_AST_LITERALS_HPP