#ifndef ARENA_INCLUDE_AST_VISITOR_HPP
#define ARENA_INCLUDE_AST_VISITOR_HPP

namespace arena::ast {
    // Forward declarations
    class Node;
    
    // Expressions
    class Expression;
    class IdExpression;
    class LiteralExpression;
    class BinaryExpression;
    class UnaryPrefixExpression;
    class DotOperatorExpression;
    class MemberAccessExpression;
    class CallExpression;
    class CastExpression;
    
    // Statements
    class Statement;
    class IfStatement;
    class LetStatement;
    class ReturnStatement;
    class ExpressionStatement;
    class BlockStatement;
    
    // Types
    class Type;
    class TypeArgument;
    class TypeArgumentType;
    class TypeArgumentLifetime;
    class NamedType;
    class PointerType;
    class ConstType;
    class ArrayType;
    
    // Declarations
    class Declaration;
    class ImportDeclaration;
    class FunctionDeclaration;
    class FunctionDefinition;
    class Parameter;
    class ParamList;
    
    // Literals
    class Literal;
    class StringLiteral;
    class IntegerLiteral;

    /**
     * Base visitor class for traversing the AST.
     * 
     * Provides virtual visit methods for all node types. Derive from this
     * class and override the methods for the nodes you want to process.
     * 
     * Example usage:
     *   class MyVisitor : public Visitor {
     *   public:
     *       void visit(BinaryExpression *expr) override {
     *           // Process binary expression
     *           expr->left->accept(this);  // Visit children
     *           expr->right->accept(this);
     *       }
     *   };
     */
    class Visitor {
    public:
        virtual ~Visitor() = default;

        // Expression visitors
        virtual void visit(const Expression *node) {}
        virtual void visit(const IdExpression *node) {}
        virtual void visit(const LiteralExpression *node) {}
        virtual void visit(const BinaryExpression *node) {}
        virtual void visit(const UnaryPrefixExpression *node) {}
        virtual void visit(const DotOperatorExpression *node) {}
        virtual void visit(const MemberAccessExpression *node) {}
        virtual void visit(const CallExpression *node) {}
        virtual void visit(const CastExpression *node) {}

        // Statement visitors
        virtual void visit(const Statement *node) {}
        virtual void visit(const IfStatement *node) {}
        virtual void visit(const LetStatement *node) {}
        virtual void visit(const ReturnStatement *node) {}
        virtual void visit(const ExpressionStatement *node) {}
        virtual void visit(const BlockStatement *node) {}

        // Type visitors
        virtual void visit(const Type *node) {}
        virtual void visit(const TypeArgument *node) {}
        virtual void visit(const TypeArgumentType *node) {}
        virtual void visit(const TypeArgumentLifetime *node) {}
        virtual void visit(const NamedType *node) {}
        virtual void visit(const PointerType *node) {}
        virtual void visit(const ConstType *node) {}
        virtual void visit(const ArrayType *node) {}

        // Declaration visitors
        virtual void visit(const Declaration *node) {}
        virtual void visit(const ImportDeclaration *node) {}
        virtual void visit(const FunctionDeclaration *node) {}
        virtual void visit(const FunctionDefinition *node) {}
        virtual void visit(const Parameter *node) {}
        virtual void visit(const ParamList *node) {}

        // Literal visitors
        virtual void visit(const Literal *node) {}
        virtual void visit(const StringLiteral *node) {}
        virtual void visit(const IntegerLiteral *node) {}
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_VISITOR_HPP