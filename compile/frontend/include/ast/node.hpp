#ifndef ARENA_INCLUDE_AST_NODE_HPP
#define ARENA_INCLUDE_AST_NODE_HPP

#include "token.hpp"

namespace arena::ast {

class Visitor;  // Forward declaration

class Node {
    public:
    Node() = default;
    Node(Token *begin, Token *end) : beginToken(begin), endToken(end) {}

    virtual ~Node() = default;

    Token *begin() { return beginToken; }
    const Token *begin() const { return beginToken; }
    Token *end() { return endToken; }
    const Token *end() const { return endToken; }

    // Accept a visitor (to be overridden by concrete nodes)
    virtual void accept(Visitor *visitor) const = 0;

    private:
    Token *beginToken;
    Token *endToken;
};

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_NODE_HPP