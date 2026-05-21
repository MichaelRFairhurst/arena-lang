#ifndef ARENA_INCLUDE_AST_STATEMENTS_HPP
#define ARENA_INCLUDE_AST_STATEMENTS_HPP

#include "ast/node.hpp"
#include "ast/types.hpp"

namespace arena::ast {

class Statement : public Node {
public:
    Statement(Token begin, Token end) : Node(begin, end) {}

    virtual ~Statement() = default;
};

class LetStatement : public Statement {
public:
    LetStatement(Token begin, Token path, Token end) : Statement(begin, end), path(path) {}

    virtual ~LetStatement() = default;

private:
    Token path;
};

} // namespace arena::ast

class

#endif // ARENA_INCLUDE_AST_STATEMENTS_HPP