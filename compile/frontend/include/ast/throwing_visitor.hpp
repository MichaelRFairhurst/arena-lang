#ifndef ARENA_INCLUDE_AST_THROWING_VISITOR_HPP
#define ARENA_INCLUDE_AST_THROWING_VISITOR_HPP
#include "ast/visitor.hpp"
#include <stdexcept>

namespace arena::ast {
    class ThrowingVisitor : public Visitor {
    public:
        ~ThrowingVisitor() override = default;

        // Expression visitors
        virtual void visit(const Expression *node) override {
            throw std::runtime_error("visit Expression not implemented");
        }
        virtual void visit(const IdExpression *node) override {
            throw std::runtime_error("visit IdExpression not implemented");
        }
        virtual void visit(const LiteralExpression *node) override {
            throw std::runtime_error("visit LiteralExpression not implemented");
        }
        virtual void visit(const BinaryExpression *node) override {
            throw std::runtime_error("visit BinaryExpression not implemented");
        }
        virtual void visit(const UnaryPrefixExpression *node) override {
            throw std::runtime_error("visit UnaryPrefixExpression not implemented");
        }
        virtual void visit(const DotOperatorExpression *node) override {
            throw std::runtime_error("visit DotOperatorExpression not implemented");
        }
        virtual void visit(const MemberAccessExpression *node) override {
            throw std::runtime_error("visit MemberAccessExpression not implemented");
        }
        virtual void visit(const CallExpression *node) override {
            throw std::runtime_error("visit CallExpression not implemented");
        }
        virtual void visit(const CastExpression *node) override {
            throw std::runtime_error("visit CastExpression not implemented");
        }

        // Statement visitors
        virtual void visit(const Statement *node) override {
            throw std::runtime_error("visit Statement not implemented");
        }
        virtual void visit(const IfStatement *node) override {
            throw std::runtime_error("visit IfStatement not implemented");
        }
        virtual void visit(const LetStatement *node) override {
            throw std::runtime_error("visit LetStatement not implemented");
        }
        virtual void visit(const ReturnStatement *node) override {
            throw std::runtime_error("visit ReturnStatement not implemented");
        }
        virtual void visit(const ExpressionStatement *node) override {
            throw std::runtime_error("visit ExpressionStatement not implemented");
        }
        virtual void visit(const BlockStatement *node) override {
            throw std::runtime_error("visit BlockStatement not implemented");
        }
        virtual void visit(const ArenaStatement *node) override {
            throw std::runtime_error("visit ArenaStatement not implemented");
        }

        // Type visitors
        virtual void visit(const Type *node) override {
            throw std::runtime_error("visit Type not implemented");
        }
        virtual void visit(const TypeArgument *node) override {
            throw std::runtime_error("visit TypeArgument not implemented");
        }
        virtual void visit(const TypeArgumentType *node) override {
            throw std::runtime_error("visit TypeArgumentType not implemented");
        }
        virtual void visit(const TypeArgumentLifetime *node) override {
            throw std::runtime_error("visit TypeArgumentLifetime not implemented");
        }
        virtual void visit(const NamedType *node) override {
            throw std::runtime_error("visit NamedType not implemented");
        }
        virtual void visit(const PointerType *node) override {
            throw std::runtime_error("visit PointerType not implemented");
        }
        virtual void visit(const ConstType *node) override {
            throw std::runtime_error("visit ConstType not implemented");
        }
        virtual void visit(const ArrayType *node) override {
            throw std::runtime_error("visit ArrayType not implemented");
        }

        // Declaration visitors
        virtual void visit(const Declaration *node) override {
            throw std::runtime_error("visit Declaration not implemented");
        }
        virtual void visit(const ImportDeclaration *node) override {
            throw std::runtime_error("visit ImportDeclaration not implemented");
        }
        virtual void visit(const FunctionDeclaration *node) override {
            throw std::runtime_error("visit FunctionDeclaration not implemented");
        }
        virtual void visit(const FunctionDefinition *node) override {
            throw std::runtime_error("visit FunctionDefinition not implemented");
        }
        virtual void visit(const Parameter *node) override {
            throw std::runtime_error("visit Parameter not implemented");
        }
        virtual void visit(const ParamList *node) override {
            throw std::runtime_error("visit ParamList not implemented");
        }
        virtual void visit(const Field *node) override {
            throw std::runtime_error("visit Field not implemented");
        }
        virtual void visit(const StructDeclaration *node) override {
            throw std::runtime_error("visit StructDeclaration not implemented");
        }
        virtual void visit(const StructDefinition *node) override {
            throw std::runtime_error("visit StructDefinition not implemented");
        }

        // Literal visitors
        virtual void visit(const Literal *node) override {
            throw std::runtime_error("visit Literal not implemented");
        }
        virtual void visit(const StringLiteral *node) override {
            throw std::runtime_error("visit StringLiteral not implemented");
        }
        virtual void visit(const IntegerLiteral *node) override {
            throw std::runtime_error("visit IntegerLiteral not implemented");
        }
    };
} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_THROWING_VISITOR_HPP