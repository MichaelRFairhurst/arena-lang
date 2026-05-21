#ifndef ARENA_INCLUDE_AST_TYPES_HPP
#define ARENA_INCLUDE_AST_TYPES_HPP

#include "ast/node.hpp"

namespace arena::ast
{
    class Literal;

    class Type : public Node
    {
    public:
        Type(Token begin, Token end) : Node(begin, end) {}

        virtual ~Type() = default;
    };

    class NamedType : public Type
    {
    public:
        NamedType(Token name) : Type(name, name), name(name) {}

        virtual ~NamedType() = default;

    private:
        Token name;
    };

    class PointerType : public Type
    {
    public:
        PointerType(Token asterisk, Type *pointee) : Type(asterisk, pointee->end()), pointee(pointee) {}

        virtual ~PointerType() = default;

    private:
        Type *pointee;
    };

    class ConstType : public Type
    {
    public:
        ConstType(Token constToken, Type *baseType) : Type(constToken, baseType->end()), baseType(baseType) {}

        virtual ~ConstType() = default;

    private:
        Type *baseType;
    };

    class ArrayType : public Type
    {
    public:
        ArrayType(Type *elementType, Token openBracket, Literal *size, Token closeBracket) : Type(elementType->begin(), closeBracket), elementType(elementType), size(size) {}

        virtual ~ArrayType() = default;

    private:
        Literal *size;
        Node *elementType;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_TYPES_HPP