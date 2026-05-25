#ifndef ARENA_INCLUDE_AST_NODE_HPP
#define ARENA_INCLUDE_AST_NODE_HPP

#include "token.hpp"

namespace arena::ast {

class Node {
    public:
    Node() = default;
    Node(Token begin, Token end) : beginToken(begin), endToken(end) {}

    virtual ~Node() = default;

    const Token &begin() const { return beginToken; }
    const Token &end() const { return endToken; }

    private:
    Token beginToken;
    Token endToken;
};

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_NODE_HPP