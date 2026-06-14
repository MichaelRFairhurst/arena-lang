#ifndef ARENA_INCLUDE_AST_STATEMENTS_HPP
#define ARENA_INCLUDE_AST_STATEMENTS_HPP

#include <string>
#include <vector>
#include "ast/node.hpp"
#include "ast/types.hpp"
#include "ast/expressions.hpp"
#include "ast/visitor.hpp"

namespace arena::ast {

    class Statement : public Node {
    public:
        Statement(Token *begin, Token *end) : Node(begin, end) {}

        virtual std::string to_string() const = 0;

        virtual ~Statement() = default;
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class IfStatement : public Statement {
    public:
        IfStatement(Token *ifToken, Expression *condition, Statement *thenBranch, Statement *elseBranch)
            : Statement(ifToken, elseBranch ? elseBranch->end() : thenBranch->end()), condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}

        virtual ~IfStatement() = default;

        std::string to_string() const override {
            std::string result = "if (" + condition->to_string() + ") " + thenBranch->to_string();
            if (elseBranch) {
                result += " else " + elseBranch->to_string();
            }
            return result;
        }
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_condition() const {
            return condition;
        }

        const Statement *get_then() const {
            return thenBranch;
        }

        const Statement *get_else() const {
            return elseBranch;
        }

        private:
        Expression *condition;
        Statement *thenBranch;
        Statement *elseBranch;
    };

    class LetStatement : public Statement {
    public:
        LetStatement(Token *let, Token *name, Type *type, Token *equalToken, Expression *initializer, Token *semicolon)
            : Statement(let, semicolon), name(name), type(type), equalToken(equalToken), initializer(initializer) {}

        virtual ~LetStatement() = default;

        std::string_view get_name() const {
            return name->text;
        }

        const Type *get_type() const {
            return type;
        }

        const Expression *get_initializer() const {
            return initializer;
        }

        std::string to_string() const override {
            std::string result = "let " + std::string(name->text);
            if (type) {
                result += ": " + type->to_string();
            }
            if (initializer) {
                result += " = " + initializer->to_string();
            }
            result += ";";
            return result;
        }
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

    private:
        Token *name;
        Type *type;
        Token *equalToken;
        Expression *initializer;
    };

    class ReturnStatement : public Statement {
    public:
        ReturnStatement(Token *returnToken, Expression *value)
            : Statement(returnToken, value->end()), value(value) {}

        virtual ~ReturnStatement() = default;

        std::string to_string() const override {
            return "return " + value->to_string() + ";";
        }
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_expr() const {
            return value;
        }

    private:
        Expression *value;
    };

    class ExpressionStatement : public Statement {
    public:
        ExpressionStatement(Token *begin, Expression *expression, Token *semicolon)
            : Statement(begin, semicolon), expression(expression) {}

        std::string to_string() const override {
            return expression->to_string() + ";";
        }

        virtual ~ExpressionStatement() = default;
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_expr() const {
            return expression;
        }

    private:
        Expression *expression;
    };

    class BlockStatement : public Statement {
    public:
        BlockStatement(Token *openBrace, std::vector<Statement *> statements, Token *closeBrace)
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
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::vector<const Statement *> get_statements() const {
            return std::vector<const Statement *>(statements.begin(), statements.end());
        }

    private:
        std::vector<Statement *> statements;
    };

    class ArenaStatement : public Statement {
    public:
        ArenaStatement(Token *arenaToken, BlockStatement *block)
            : Statement(arenaToken, block->end()), block(block) {}

        virtual ~ArenaStatement() = default;
        
        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string to_string() const override {
            return "arena " + block->to_string();
        }

        const BlockStatement *get_block() const {
            return block;
        }

    private:
        BlockStatement *block;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_STATEMENTS_HPP