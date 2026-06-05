#ifndef ARENA_INCLUDE_AST_TYPES_HPP
#define ARENA_INCLUDE_AST_TYPES_HPP

#include <vector>
#include "ast/node.hpp"
#include "ast/literals.hpp"
#include "ast/visitor.hpp"

namespace arena::ast {
    class Type : public Node {
    public:
        Type(Token *begin, Token *end) : Node(begin, end) {}

        virtual std::string to_string() const = 0;

        virtual ~Type() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class TypeArgument : public Node {
    public:
        TypeArgument(Token *begin, Token *end) : Node(begin, end) {}

        virtual std::string to_string() const = 0;

        virtual ~TypeArgument() = default;

        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class TypeArgumentType : public TypeArgument {
    public:
        TypeArgumentType(Token *name, Type *type)
            : TypeArgument(name, type->end()), name(name), type(type) {}

        virtual ~TypeArgumentType() = default;

        std::string to_string() const override { return type->to_string(); }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return name->text; }

    private:
        Token *name;
        Type *type;
    };

    class TypeArgumentLifetime : public TypeArgument {
    public:
        TypeArgumentLifetime(Token *star, Token *name)
            : TypeArgument(star, name), star(star), name(name) {}

        virtual ~TypeArgumentLifetime() = default;

        std::string to_string() const override { return "*" + std::string(name->text); }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return name->text; }

    private:
        Token *star;
        Token *name;
    };


    class NamedType : public Type {
    public:
        NamedType(Token *name, std::vector<TypeArgument *> genericArgs)
            : Type(name, name), name(name), genericArgs(genericArgs) {}

        virtual ~NamedType() = default;

        std::string to_string() const override {
            std::string result = std::string(name->text);
            if (!genericArgs.empty()) {
                result += "<";
                for (size_t i = 0; i < genericArgs.size(); ++i) {
                    result += genericArgs[i]->to_string();
                    if (i < genericArgs.size() - 1) {
                        result += ", ";
                    }
                }
                result += ">";
            }
            return result;
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        std::string_view get_name() const { return name->text; }

    private:
        Token *name;
        std::vector<TypeArgument *> genericArgs;
    };

    class PointerType : public Type {
    public:
        PointerType(Token *asterisk, Type *pointee, Token *lifetime /*optional*/)
            : Type(asterisk, lifetime == nullptr ? pointee->end() : lifetime), pointee(pointee),
              lifetime(lifetime) {}

        virtual ~PointerType() = default;

        std::string to_string() const override {
            std::string result = pointee->to_string();
            if (lifetime) {
                result += " *" + std::string(lifetime->text);
            } else {
                result += "*";
            }
            return result;
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Type *get_pointee() const { return pointee; }

        std::optional<std::string_view> get_lifetime() const {
            if (lifetime) {
                return lifetime->text;
            }
            return std::nullopt;
        }

    private:
        Type *pointee;
        Token *lifetime; // optional
    };

    class ConstType : public Type {
    public:
        ConstType(Token *constToken, Type *baseType)
            : Type(constToken, baseType->end()), baseType(baseType) {}

        virtual ~ConstType() = default;

        const Type *get_base_type() const { return baseType; }

    private:
        Type *baseType;

        std::string to_string() const override { return "const " + baseType->to_string(); }

        void accept(Visitor *visitor) const override { visitor->visit(this); }
    };

    class ArrayType : public Type {
    public:
        ArrayType(Type *elementType, Token *openBracket, Literal *size, Token *closeBracket)
            : Type(elementType->begin(), closeBracket), elementType(elementType), size(size) {}

        virtual ~ArrayType() = default;

        std::string to_string() const override {
            return elementType->to_string() + "[" + size->to_string() + "]";
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        const Type *get_element_type() const { return elementType; }

        const Literal *get_size_literal() const { return size; }

    private:
        Literal *size;
        Type *elementType;
    };

} // namespace arena::ast

#endif // ARENA_INCLUDE_AST_TYPES_HPP