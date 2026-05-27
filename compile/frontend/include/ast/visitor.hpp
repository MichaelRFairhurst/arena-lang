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
    class Argument;
    class ArgList;
    
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
        virtual void visit(Expression *node) {}
        virtual void visit(IdExpression *node) {}
        virtual void visit(LiteralExpression *node) {}
        virtual void visit(BinaryExpression *node) {}
        virtual void visit(UnaryPrefixExpression *node) {}
        virtual void visit(DotOperatorExpression *node) {}
        virtual void visit(MemberAccessExpression *node) {}
        virtual void visit(CallExpression *node) {}
        virtual void visit(CastExpression *node) {}

        // Statement visitors
        virtual void visit(Statement *node) {}
        virtual void visit(IfStatement *node) {}
        virtual void visit(LetStatement *node) {}
        virtual void visit(ReturnStatement *node) {}
        virtual void visit(ExpressionStatement *node) {}
        virtual void visit(BlockStatement *node) {}

        // Type visitors
        virtual void visit(Type *node) {}
        virtual void visit(TypeArgument *node) {}
        virtual void visit(TypeArgumentType *node) {}
        virtual void visit(TypeArgumentLifetime *node) {}
        virtual void visit(NamedType *node) {}
        virtual void visit(PointerType *node) {}
        virtual void visit(ConstType *node) {}
        virtual void visit(ArrayType *node) {}

        // Declaration visitors
        virtual void visit(Declaration *node) {}
        virtual void visit(ImportDeclaration *node) {}
        virtual void visit(FunctionDeclaration *node) {}
        virtual void visit(FunctionDefinition *node) {}
        virtual void visit(Argument *node) {}
        virtual void visit(ArgList *node) {}

        // Literal visitors
        virtual void visit(Literal *node) {}
        virtual void visit(StringLiteral *node) {}
        virtual void visit(IntegerLiteral *node) {}
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_VISITOR_HPP