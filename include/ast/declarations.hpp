#ifndef ARENA_INCLUDE_AST_DECLARATIONS_HPP
#define ARENA_INCLUDE_AST_DECLARATIONS_HPP

#include <vector>
#include "ast/node.hpp"
#include "ast/types.hpp"
#include "ast/statements.hpp"

namespace arena::ast {

    class Declaration : public Node {
    public:
        Declaration(Token begin, Token end) : Node(begin, end) {}

        virtual ~Declaration() = default;
        virtual std::string to_string() const = 0;
    };

    class ImportDeclaration : public Declaration {
    public:
        ImportDeclaration(Token includeToken, Token path, Token semicolon)
            : Declaration(includeToken, semicolon), path(path) {}

        virtual ~ImportDeclaration() = default;

        std::string to_string() const override {
            return "import " + std::string(path.text) + ";";
        }

    private:
        Token path;
    };

    class Argument : public Node {
    public:
        Argument(Token name, Type *type) : Node(name, type->end()), name(name), type(type) {}

        virtual ~Argument() = default;

        std::string to_string() const {
            return std::string(name.text) + ": " + type->to_string();
        }

    private:
        Token name;
        Type *type;
    };

    class ArgList : public Node {
    public:
        ArgList(Token openParen, std::vector<Argument *> arguments, Token closeParen)
            : Node(openParen, closeParen), openParen(openParen), closeParen(closeParen),
              arguments(arguments) {}

        virtual ~ArgList() = default;

        std::string to_string() const {
            std::string result = "(";
            for (size_t i = 0; i < arguments.size(); ++i) {
                result += arguments[i]->to_string();
                if (i < arguments.size() - 1) {
                    result += ", ";
                }
            }
            result += ")";
            return result;
        }

    private:
        Token openParen;
        Token closeParen;
        std::vector<Argument *> arguments;
    };

    class FunctionDeclaration : public Declaration {
    public:
        FunctionDeclaration(Token funToken,
                            Token name,
                            ArgList *args,
                            Token *returnArrow,
                            Type *returnType,
                            Token endToken)
            : Declaration(funToken, endToken), name(name), args(args), returnArrow(returnArrow),
              returnType(returnType) {}

        virtual ~FunctionDeclaration() = default;

        std::string to_string() const override {
            std::string result = "fun " + std::string(name.text) + args->to_string();
            if (returnType) {
                result += " -> " + returnType->to_string();
            }
            return result + ";";
        }

        const Type *get_return_type() const {
            return returnType;
        }

        const Token &get_name_token() const {
            return name;
        }

        const ArgList *get_args() const {
            return args;
        }

    private:
        Token name;
        ArgList *args;
        Token *returnArrow;
        Type *returnType;
    };

    class FunctionDefinition : public FunctionDeclaration {
    public:
        FunctionDefinition(Token funToken,
                          Token name,
                          ArgList *args,
                          Token *returnArrow,
                          Type *returnType,
                          BlockStatement *body)
            : FunctionDeclaration(funToken, name, args, returnArrow, returnType, body->end()),
              body(body) {}

        virtual ~FunctionDefinition() = default;

        std::string to_string() const override {
            std::string result = "fun " + std::string(get_name_token().text) + get_args()->to_string();
            auto return_type = get_return_type();
            if (return_type) {
                result += " -> " + return_type->to_string();
            }
            return result + body->to_string();
        }

    private:
        BlockStatement *body;
    };


} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_DECLARATIONS_HPP