#ifndef ARENA_INCLUDE_AST_DECLARATIONS_HPP
#define ARENA_INCLUDE_AST_DECLARATIONS_HPP

#include <vector>
#include "ast/node.hpp"
#include "ast/types.hpp"
#include "ast/statements.hpp"
#include "ast/visitor.hpp"

namespace arena::ast {

    class Declaration : public Node {
    public:
        Declaration(Token *begin, Token *end) : Node(begin, end) {}

        virtual ~Declaration() = default;
        virtual std::string to_string() const = 0;

        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class ImportDeclaration : public Declaration {
    public:
        ImportDeclaration(Token *includeToken, Token *path, Token *semicolon)
            : Declaration(includeToken, semicolon), path(path) {}

        virtual ~ImportDeclaration() = default;

        std::string to_string() const override { return "import " + std::string(path->text) + ";"; }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_path() const { return path->text; }

    private:
        Token *path;
    };

    class Parameter : public Node {
    public:
        Parameter(Token *name, Type *type) : Node(name, type->end()), name(name), type(type) {}

        virtual ~Parameter() = default;

        std::string to_string() const { return std::string(name->text) + ": " + type->to_string(); }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return name->text; }

        Type *get_type() const { return type; }

    private:
        Token *name;
        Type *type;
    };

    class ParamList : public Node {
    public:
        ParamList(Token *openParen, std::vector<Parameter *> arguments, Token *closeParen)
            : Node(openParen, closeParen), openParen(openParen), closeParen(closeParen),
              arguments(arguments) {}

        virtual ~ParamList() = default;

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

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const std::vector<Parameter *> &get_params() const { return arguments; }

    private:
        Token *openParen;
        Token *closeParen;
        std::vector<Parameter *> arguments;
    };

    class FunctionDeclaration : public Declaration {
    public:
        FunctionDeclaration(Token *funToken,
                            Token *name,
                            ParamList *params,
                            Token *returnArrow, // optional
                            Type *returnType,   // optional
                            Token *endToken)
            : Declaration(funToken, endToken), name(name), params(params), returnArrow(returnArrow),
              returnType(returnType) {}

        virtual ~FunctionDeclaration() = default;

        std::string to_string() const override {
            std::string result = "fun " + std::string(name->text) + params->to_string();
            if (returnType) {
                result += " -> " + returnType->to_string();
            }
            return result + ";";
        }

        const Type *get_return_type() const { return returnType; }

        const Token *get_name_token() const { return name; }

        std::string_view get_name() const { return name->text; }

        const ParamList *get_params() const { return params; }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

    private:
        Token *name;
        ParamList *params;
        Token *returnArrow; // optional
        Type *returnType;   // optional
    };

    class FunctionDefinition : public FunctionDeclaration {
    public:
        FunctionDefinition(Token *funToken,
                           Token *name,
                           ParamList *params,
                           Token *returnArrow, // optional
                           Type *returnType,   // optional
                           BlockStatement *body)
            : FunctionDeclaration(funToken, name, params, returnArrow, returnType, body->end()),
              body(body) {}

        virtual ~FunctionDefinition() = default;

        std::string to_string() const override {
            std::string result =
                "fun " + std::string(get_name_token()->text) + get_params()->to_string();
            auto return_type = get_return_type();
            if (return_type) {
                result += " -> " + return_type->to_string();
            }
            return result + body->to_string();
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const BlockStatement *get_body() const { return body; }

    private:
        BlockStatement *body;
    };

    class Field : public Node {
    public:
        Field(Token *name, Token *colon, Type *type, Token *semicolon)
            : Node(name, semicolon), colon(colon), type(type) {}

        virtual ~Field() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return begin()->text; }

        const Type *get_type() const { return type; }

        std::string to_string() const {
            return std::string(get_name()) + ": " + type->to_string() + ";";
        }

    private:
        Token *colon;
        Type *type;
    };

    class StructDeclaration : public Declaration {
    public:
        StructDeclaration(Token *structKeyword, Token *name, Token *endToken)
            : Declaration(structKeyword, endToken), name(name) {}

        virtual ~StructDeclaration() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return name->text; }

        std::string to_string() const override {
            return "struct " + std::string(get_name()) + ";";
        }

    private:
        Token *name;
    };

    class StructDefinition : public StructDeclaration {
    public:
        StructDefinition(Token *structKeyword,
                         Token *name,
                         Token *openBrace,
                         std::vector<Field *> fields,
                         Token *closeBrace)
            : StructDeclaration(structKeyword, name, closeBrace), openBrace(openBrace),
              fields(std::move(fields)) {}

        virtual ~StructDefinition() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const std::vector<Field *> *get_fields() const { return &fields; }

        std::string to_string() const override {
            std::string result = "struct " + std::string(get_name()) + ";";
            result += util::concat(fields, " ") + "}";
            return result;
        }

    private:
        Token *openBrace;
        std::vector<Field *> fields;
    };


} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_DECLARATIONS_HPP