#ifndef ARENA_INCLUDE_AST_EXPRESSIONS_HPP
#define ARENA_INCLUDE_AST_EXPRESSIONS_HPP

#include <vector>
#include "ast/node.hpp"
#include "ast/types.hpp"
#include "ast/literals.hpp"
#include "ast/visitor.hpp"
#include "util/concat.hpp"

namespace arena::ast {
    class Expression : public Node {
    public:
        Expression() = default;
        Expression(Token *begin, Token *end) : Node(begin, end) {}

        virtual ~Expression() = default;

        virtual std::string to_string() const = 0;

        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class IdExpression : public Expression {
    public:
        IdExpression() = default;
        IdExpression(Token *name) : Expression(name, name), name(name) {}
        IdExpression(const IdExpression &other) = default;
        IdExpression(IdExpression &&other) = default;

        virtual ~IdExpression() = default;

        virtual std::string to_string() const override { return std::string(name->text); }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_id() const { return std::string_view(name->text); }

    private:
        Token *name;
    };

    class LiteralExpression : public Expression {
    public:
        LiteralExpression(Literal *literal)
            : Expression(literal->begin(), literal->end()), literal(literal) {}

        virtual ~LiteralExpression() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }

    private:
        Literal *literal;
    };

    class BinaryExpression : public Expression {
    public:
        BinaryExpression() = default;
        BinaryExpression(Expression *left, Token *op, Expression *right)
            : Expression(left->begin(), right->end()), op(op), left(left), right(right) {}
        BinaryExpression(const BinaryExpression &other) = default;
        BinaryExpression(BinaryExpression &&other) = default;

        virtual ~BinaryExpression() = default;

        std::string to_string() const override {
            return "(" + left->to_string() + " " + std::string(op->text) + " " +
                   right->to_string() + ")";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_left() const {
            return left;
        }

        const Expression *get_right() const {
            return right;
        }

    private:
        Token *op;
        Expression *left;
        Expression *right;
    };

    class UnaryPrefixExpression : public Expression {
    public:
        UnaryPrefixExpression(Token *op, Expression *operand)
            : Expression(op, operand->end()), op(op), operand(operand) {}
        virtual ~UnaryPrefixExpression() = default;


        std::string to_string() const override {
            return "(" + std::string(op->text) + operand->to_string() + ")";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_operand() const {
            return operand;
        }

        const Token* get_operator_token() const {
            return op;
        }

        TokenType get_operator() const { return op->type; }

    private:
        Token *op;
        Expression *operand;
    };

    class DotOperatorExpression : public Expression {
    public:
        DotOperatorExpression(Expression *operand, Token *dot, Token *op)
            : Expression(operand->begin(), op), operand(operand), dot(dot), op(op) {}

        virtual ~DotOperatorExpression() = default;

        std::string to_string() const override {
            return "(" + operand->to_string() + " " + std::string(dot->text) + " " +
                   std::string(op->text) + ")";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_operand() const {
            return operand;
        }

        TokenType get_operator() const {
            return op->type;
        }

        const Token *get_operator_token() const {
            return op;
        }

    private:
        Expression *operand;
        Token *dot;
        Token *op;
    };

    class MemberAccessExpression : public Expression {
    public:
        MemberAccessExpression(Expression *object, Token *dot, Token *member)
            : Expression(object->begin(), member), object(object), dot(dot), member(member) {}
        virtual ~MemberAccessExpression() = default;

        std::string to_string() const override {
            return "(" + object->to_string() + " " + std::string(dot->text) + " " +
                   std::string(member->text) + ")";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_object() const {
            return object;
        }

    private:
        Expression *object;
        Token *dot;
        Token *member;
    };

    class CallExpression : public Expression {
    public:
        CallExpression(Expression *callee,
                       Token *openParen,
                       std::vector<Expression *> args,
                       Token *closeParen)
            : Expression(callee->begin(), closeParen), callee(callee), openParen(openParen),
              args(args), closeParen(closeParen) {}

        virtual ~CallExpression() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string to_string() const override {
            return callee->to_string() + "(" + util::concat(args, ", ") + ")";
        }

        const Expression *get_callee() const {
            return callee;
        }

        const std::vector<Expression *> &get_args() const {
            return args;
        }

    private:
        Expression *callee;
        Token *openParen;
        std::vector<Expression *> args;
        Token *closeParen;
    };

    class CastExpression : public Expression {
    public:
        CastExpression(Expression *expr,
                       Token *dotToken,
                       Token *asToken,
                       Token *openParen,
                       Type *targetType,
                       Token *closeParen)
            : Expression(expr->begin(), closeParen), expr(expr), dotToken(dotToken),
              asToken(asToken), openParen(openParen), targetType(targetType),
              closeParen(closeParen) {}

        virtual ~CastExpression() = default;

        std::string to_string() const override {
            return "(" + expr->to_string() + " " + std::string(dotToken->text) + " " +
                   std::string(asToken->text) + " (" + targetType->to_string() + "))";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Expression *get_expr() const {
            return expr;
        }

        const Type* get_type() const {
            return targetType;
        }

    private:
        Expression *expr;
        Token *dotToken;
        Token *asToken;
        Token *openParen;
        Type *targetType;
        Token *closeParen;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_EXPRESSIONS_HPP