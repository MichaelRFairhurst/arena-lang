#ifndef ARENA_INCLUDE_AST_DECLARATIONS_HPP
#define ARENA_INCLUDE_AST_DECLARATIONS_HPP

#include <vector>
#include "ast/node.hpp"
#include "ast/types.hpp"

namespace arena::ast {

    class Declaration : public Node {
    public:
        Declaration(Token begin, Token end) : Node(begin, end) {}

        virtual ~Declaration() = default;
    };

    class ImportDeclaration : public Declaration {
    public:
        ImportDeclaration(Token includeToken, Token path, Token semicolon)
            : Declaration(includeToken, semicolon), path(path) {}

        virtual ~ImportDeclaration() = default;

    private:
        Token path;
    };

    class Argument : public Node {
    public:
        Argument(Token name, Type *type) : Node(name, type->end()), name(name), type(type) {}

        virtual ~Argument() = default;

    private:
        Token name;
        Type *type;
    };

    class ArgList : public Node {
    public:
        ArgList(std::vector<Argument *> arguments)
            : Node(arguments.front()->begin(), arguments.back()->end()), arguments(arguments) {}

    private:
        std::vector<Argument *> arguments;

        virtual ~ArgList() = default;
    };

    class FunctionDeclaration : public Declaration {
    public:
        FunctionDeclaration(
            Token funcToken, Token name, Token openParen, ArgList *args, Token closeParen)
            : Declaration(funcToken, closeParen), name(name), args(args) {}

        virtual ~FunctionDeclaration() = default;

    private:
        Token name;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_DECLARATIONS_HPP