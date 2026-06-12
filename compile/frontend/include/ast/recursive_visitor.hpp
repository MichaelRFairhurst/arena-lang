#ifndef ARENA_INCLUDE_AST_RECURSIVE_VISITOR_HPP
#define ARENA_INCLUDE_AST_RECURSIVE_VISITOR_HPP

#include "ast/throwing_visitor.hpp"
#include "ast/expressions.hpp"
#include "ast/statements.hpp"
#include "ast/types.hpp"
#include "ast/declarations.hpp"

namespace arena::ast {
    class RecursiveVisitor : public ThrowingVisitor {
    public:
        ~RecursiveVisitor() override = default;

        // Expression visitors
        void visit(const Expression *node) override {}
        void visit(const IdExpression *node) override {}
        void visit(const LiteralExpression *node) override {}
        void visit(const BinaryExpression *node) override {
            node->get_left()->accept(this);
            node->get_right()->accept(this);
        }
        void visit(const UnaryPrefixExpression *node) override {
            node->get_operand()->accept(this);
        }
        void visit(const DotOperatorExpression *node) override {
            node->get_operand()->accept(this);
        }
        void visit(const MemberAccessExpression *node) override {
            node->get_object()->accept(this);
        }
        void visit(const CallExpression *node) override {
            node->get_callee()->accept(this);
            for (const auto *arg : node->get_args()) {
                arg->accept(this);
            }
        }
        void visit(const CastExpression *node) override {
            node->get_expr()->accept(this);
            node->get_type()->accept(this);
        }

        // Statement visitors
        void visit(const Statement *node) override {}
        void visit(const IfStatement *node) override {
            node->get_condition()->accept(this);
            node->get_then()->accept(this);
            if (node->get_else()) {
                node->get_else()->accept(this);
            }
        }

        void visit(const LetStatement *node) override {
            node->get_type()->accept(this);
            if (node->get_initializer()) {
                node->get_initializer()->accept(this);
            }
        }

        void visit(const ReturnStatement *node) override {
            if (node->get_expr()) {
                node->get_expr()->accept(this);
            }
        }
        void visit(const ExpressionStatement *node) override { node->get_expr()->accept(this); }
        void visit(const BlockStatement *node) override {
            for (const auto *stmt : node->get_statements()) {
                stmt->accept(this);
            }
        }
        void visit(const ArenaStatement *node) override { node->get_block()->accept(this); }

        // Type visitors
        void visit(const Type *node) override {}
        void visit(const TypeArgumentType *node) override { node->get_type()->accept(this); }

        void visit(const TypeArgumentLifetime *node) override {}

        void visit(const NamedType *node) override {
            for (const auto *arg : node->get_generic_args()) {
                arg->accept(this);
            }
        }

        void visit(const PointerType *node) override { node->get_pointee()->accept(this); }

        void visit(const ConstType *node) override { node->get_base_type()->accept(this); }

        void visit(const ArrayType *node) override {
            node->get_element_type()->accept(this);
            node->get_size_literal()->accept(this);
        }

        // Declaration visitors
        virtual void visit(const Declaration *node) override {}
        virtual void visit(const ImportDeclaration *node) override {}
        virtual void visit(const FunctionDeclaration *node) override {
            for (const auto *param : node->get_params()->get_params()) {
                param->accept(this);
            }
            if (node->get_return_type()) {
                node->get_return_type()->accept(this);
            }
        }

        virtual void visit(const FunctionDefinition *node) override {
            visit(static_cast<const FunctionDeclaration *>(node));
            node->get_body()->accept(this);
        }

        virtual void visit(const Parameter *node) override { node->get_type()->accept(this); }

        virtual void visit(const ParamList *node) override {
            for (const auto *param : node->get_params()) {
                param->accept(this);
            }
        }

        virtual void visit(const Field *node) override { node->get_type()->accept(this); }

        virtual void visit(const StructDeclaration *node) override {}
        virtual void visit(const StructDefinition *node) override {
            for (const auto *field : *node->get_fields()) {
                field->accept(this);
            }
        }

        // Literal visitors
        virtual void visit(const Literal *node) override {}
        virtual void visit(const StringLiteral *node) override {}
        virtual void visit(const IntegerLiteral *node) override {}
    };
} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_RECURSIVE_VISITOR_HPP