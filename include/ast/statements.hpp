#ifndef ARENA_INCLUDE_AST_STATEMENTS_HPP
#define ARENA_INCLUDE_AST_STATEMENTS_HPP

#include "ast/node.hpp"
#include "ast/types.hpp"
#include "ast/expressions.hpp"

namespace arena::ast {

    class Statement : public Node {
    public:
        Statement(Token begin, Token end) : Node(begin, end) {}

        virtual std::string to_string() const = 0;

        virtual ~Statement() = default;
    };

    class IfStatement : public Statement {
    public:
        IfStatement(Token ifToken, Expression *condition, Statement *thenBranch, Statement *elseBranch)
            : Statement(ifToken, elseBranch ? elseBranch->end() : thenBranch->end()), condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}

        virtual ~IfStatement() = default;

        std::string to_string() const override {
            std::string result = "if (" + condition->to_string() + ") " + thenBranch->to_string();
            if (elseBranch) {
                result += " else " + elseBranch->to_string();
            }
            return result;
        }

        private:
        Expression *condition;
        Statement *thenBranch;
        Statement *elseBranch;
    };

    class LetStatement : public Statement {
    public:
        LetStatement(Token let, Token name, Type *type)
            : Statement(let, type == nullptr ? name : type->end()), name(name), type(type) {}

        virtual ~LetStatement() = default;

        std::string to_string() const override {
            std::string result = "let " + std::string(name.text);
            if (type) {
                result += ": " + type->to_string();
            }
            result += ";";
            return result;
        }

    private:
        Token name;
        Type *type;
    };

    class ReturnStatement : public Statement {
    public:
        ReturnStatement(Token returnToken, Expression *value)
            : Statement(returnToken, value->end()), value(value) {}

        virtual ~ReturnStatement() = default;

        std::string to_string() const override {
            return "return " + value->to_string() + ";";
        }

    private:
        Expression *value;
    };

    class ExpressionStatement : public Statement {
    public:
        ExpressionStatement(Token begin, Expression *expression, Token semicolon)
            : Statement(begin, semicolon), expression(expression) {}

        std::string to_string() const override {
            return expression->to_string() + ";";
        }

        virtual ~ExpressionStatement() = default;

    private:
        Expression *expression;
    };

    class BlockStatement : public Statement {
    public:
        BlockStatement(Token openBrace, std::vector<Statement *> statements, Token closeBrace)
            : Statement(openBrace, closeBrace), statements(statements) {}

        std::string to_string() const override {
            std::string result = "{";
            for (auto stmt : statements) {
                result += stmt->to_string();
            }
            result += "}";
            return result;
        }

        virtual ~BlockStatement() = default;

    private:
        std::vector<Statement *> statements;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_STATEMENTS_HPP