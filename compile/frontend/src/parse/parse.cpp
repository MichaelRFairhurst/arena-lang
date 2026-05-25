#include <string>
#include <string_view>
#include <algorithm>
#include <utility>
#include <optional>
#include <iostream>
#include <cassert>

#include "ast/token.hpp"
#include "parse/parse.hpp"

extern "C" {
#include "arena.h"
rena_arena ast_arena;
}

namespace {
    template <typename T, typename... Args>
    T *ast_arena_new(Args &&...args) {
        void *ptr;
        if (rena_arena_alloc(&ast_arena, sizeof(T), alignof(T), &ptr) != RENA_ARENA_SUCCESS) {
            return nullptr;
        }
        return new (ptr) T(std::forward<Args>(args)...);
    }
} // namespace

namespace arena::parse {
    using namespace arena::ast;

    class TokenIterator {
        std::string_view state;
        Token* prev = nullptr;
        Token* head = nullptr;

        Token* set_head(TokenType type, std::string_view text) {
            prev = head;
            head = ast_arena_new<Token>(Token{type, text, nullptr});

            if (prev != nullptr) {
                prev->next = head;
            }

            return head;
        }

    public:
        TokenIterator(std::string_view input) : state(input) {}

        Token* peek() {
            if (head == nullptr) {
                next();
            }
            return head;
        }

        Token* take() {
            if (head == nullptr) {
                next();
            }
            next();
            return prev;
        }

        Token* next() {
            auto begin = std::find_if_not(state.begin(), state.end(), isspace);

            state = std::string_view(begin, state.end() - begin);
            if (state.empty()) {
                return set_head(TokenType::END_OF_INPUT, state);
            }

            std::optional<TokenType> matchType;

            if (state.size() > 1) {
                // Check for multi-character tokens first
                std::string_view twoCharToken = state.substr(0, 2);
                if (twoCharToken == "&&") {
                    matchType = TokenType::AND;
                } else if (twoCharToken == "||") {
                    matchType = TokenType::OR;
                } else if (twoCharToken == "==") {
                    matchType = TokenType::EQUAL_EQUAL;
                } else if (twoCharToken == "!=") {
                    matchType = TokenType::NOT_EQUAL;
                } else if (twoCharToken == "<=") {
                    matchType = TokenType::LESS_EQUAL;
                } else if (twoCharToken == ">=") {
                    matchType = TokenType::GREATER_EQUAL;
                } else if (twoCharToken == "->") {
                    matchType = TokenType::ARROW;
                }

                if (matchType.has_value()) {
                    state.remove_prefix(2);
                    return set_head(matchType.value(), twoCharToken);
                }
            }

            std::string_view tokenText = state.substr(0, 1);

            switch (state[0]) {
            case '+':
                matchType = TokenType::PLUS;
                break;
            case '-':
                matchType = TokenType::MINUS;
                break;
            case '*':
                matchType = TokenType::STAR;
                break;
            case '/':
                matchType = TokenType::SLASH;
                break;
            case '=':
                matchType = TokenType::EQUAL;
                break;
            case '(':
                matchType = TokenType::OPEN_PAREN;
                break;
            case ')':
                matchType = TokenType::CLOSE_PAREN;
                break;
            case '[':
                matchType = TokenType::OPEN_BRACKET;
                break;
            case ']':
                matchType = TokenType::CLOSE_BRACKET;
                break;
            case '<':
                matchType = TokenType::LESS;
                break;
            case '>':
                matchType = TokenType::GREATER;
                break;
            case '.':
                matchType = TokenType::DOT;
                break;
            case '!':
                matchType = TokenType::NOT;
                break;
            case '&':
                matchType = TokenType::AMP;
                break;
            case '{':
                matchType = TokenType::OPEN_BRACE;
                break;
            case '}':
                matchType = TokenType::CLOSE_BRACE;
                break;
            case ':':
                matchType = TokenType::COLON;
                break;
            case ';':
                matchType = TokenType::SEMICOLON;
                break;
            case ',':
                matchType = TokenType::COMMA;
                break;
            default:
                // fall through
                ;
            }

            if (matchType.has_value()) {
                state.remove_prefix(1);
                return set_head(matchType.value(), tokenText);
            }

            // TODO: handle integer literals, string literals, comments, etc.

            auto alphaNumEnd = std::find_if_not(state.begin(), state.end(), [](char c) {
                return std::isalnum(c) || c == '_';
            });

            tokenText = std::string_view(state.data(), alphaNumEnd - state.begin());
            std::cout << tokenText << std::endl;
            state = std::string_view(alphaNumEnd, state.end() - alphaNumEnd);
            std::cout << state << std::endl;

            if (tokenText == "as") {
                return set_head(TokenType::AS, tokenText);
            } else if (tokenText == "fun") {
                return set_head(TokenType::FUN, tokenText);
            } else if (tokenText == "true") {
                return set_head(TokenType::TRUE, tokenText);
            } else if (tokenText == "false") {
                return set_head(TokenType::FALSE, tokenText);
            } else if (tokenText == "if") {
                return set_head(TokenType::IF, tokenText);
            } else if (tokenText == "else") {
                return set_head(TokenType::ELSE, tokenText);
            } else if (tokenText == "for") {
                return set_head(TokenType::FOR, tokenText);
            } else if (tokenText == "while") {
                return set_head(TokenType::WHILE, tokenText);
            } else if (tokenText == "let") {
                return set_head(TokenType::LET, tokenText);
            } else if (tokenText == "ret") {
                return set_head(TokenType::RET, tokenText);
            } else if (tokenText == "import") {
                return set_head(TokenType::IMPORT, tokenText);
            } else if (tokenText == "struct") {
                return set_head(TokenType::STRUCT, tokenText);
            }

            return set_head(TokenType::IDENTIFIER, tokenText);
        }
    };

    Literal *parse_literal(TokenIterator &tokens);
    Statement *parse_statement(TokenIterator &tokens);
    Expression *parse_expression(TokenIterator &tokens);
    Type *parse_type(TokenIterator &tokens);

    std::vector<TypeArgument *> parse_generic_args(TokenIterator &tokens) {
        assert(tokens.peek()->type == TokenType::LESS);
        tokens.take();
        std::vector<TypeArgument *> generic_args;
        while (true) {
            if (tokens.peek()->type == TokenType::STAR) {
                Token *asterisk = tokens.take(); // consume '*'
                Token *lifetime = tokens.take(); // consume lifetime identifier
                if (lifetime->type != TokenType::IDENTIFIER) {
                    throw std::runtime_error("Expected lifetime identifier but got: " +
                                             std::string(lifetime->text));
                }
                generic_args.push_back(ast_arena_new<TypeArgumentLifetime>(asterisk, lifetime));
            } else {
                Type *given_type = parse_type(tokens);
                generic_args.push_back(ast_arena_new<TypeArgumentType>(given_type->begin(), given_type));
            }

            if (tokens.peek()->type == TokenType::COMMA) {
                tokens.take(); // consume ','
            } else if (tokens.peek()->type == TokenType::GREATER) {
                tokens.take(); // consume '>'
                break;
            } else {
                throw std::runtime_error("Expected ',' or '>' in generic argument list but got: " +
                                         std::string(tokens.peek()->text));
            }
        }

        return generic_args;
    }

    Type *parse_type(TokenIterator &tokens) {
        // TODO: const types
        Type *type;
        if (tokens.peek()->type != TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected type name but got: " +
                                     std::string(tokens.peek()->text));
        }

        Token *name = tokens.take();
        if (tokens.peek()->type == TokenType::LESS) {
            std::vector<TypeArgument *> generic_args = parse_generic_args(tokens);
            type = ast_arena_new<NamedType>(name, generic_args);
        } else {
            type = ast_arena_new<NamedType>(name, std::vector<TypeArgument *>{});
        }

        do {
            if (tokens.peek()->type == TokenType::STAR) {
                Token *asterisk = tokens.take();
                if (tokens.peek()->type == TokenType::IDENTIFIER) {
                    Token *lifetime = tokens.take();
                    type = ast_arena_new<PointerType>(asterisk, type, lifetime);
                    std::cout << "Parsed pointer type with lifetime: " << lifetime->text
                              << std::endl;
                } else {
                    type = ast_arena_new<PointerType>(asterisk, type, nullptr);
                }
            } else if (tokens.peek()->type == TokenType::OPEN_BRACKET) {
                Token *openBracket = tokens.take();
                Literal *size = parse_literal(tokens);
                if (tokens.peek()->type != TokenType::CLOSE_BRACKET) {
                    throw std::runtime_error("Expected ']' but got: " +
                                             std::string(tokens.peek()->text));
                }
                Token *closeBracket = tokens.take();
                type = ast_arena_new<ArrayType>(type, openBracket, size, closeBracket);
            } else {
                break;
            }
        } while (true);

        return type;
    }

    Literal *parse_literal(TokenIterator &tokens) {
        // TODO: actually parse different literals
        if (tokens.peek()->type != TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected literal but got: " +
                                     std::string(tokens.peek()->text));
        }

        return ast_arena_new<Literal>(tokens.take());
    }

    Expression *parse_primary_expression(TokenIterator &tokens,
                                         std::vector<TokenType> stopTokens = {}) {
        if (tokens.peek()->type == TokenType::IDENTIFIER) {
            return ast_arena_new<IdExpression>(tokens.take());
        } else if (tokens.peek()->type == TokenType::OPEN_PAREN) {
            Token *openParen = tokens.take();
            Expression *expr = parse_expression(tokens);
            if (tokens.peek()->type != TokenType::CLOSE_PAREN) {
                throw std::runtime_error("Expected ')' but got: " +
                                         std::string(tokens.peek()->text));
            }
            tokens.take(); // consume close paren
            return expr;
        } else {
            throw std::runtime_error("Expected identifier, but got " +
                                     std::string(tokens.peek()->text));
        }
    }

    Expression *parse_dot_expression(TokenIterator &tokens) {
        Expression *expr = parse_primary_expression(tokens);

        while (tokens.peek()->type == TokenType::DOT) {
            Token *dot = tokens.take();
            if (tokens.peek()->type == TokenType::IDENTIFIER) {
                Token *member = tokens.take();
                expr = ast_arena_new<MemberAccessExpression>(expr, dot, member);
            } else if (tokens.peek()->type == TokenType::STAR ||
                       tokens.peek()->type == TokenType::AMP) {
                Token *op = tokens.take();
                expr = ast_arena_new<DotOperatorExpression>(expr, dot, op);
            } else if (tokens.peek()->type == TokenType::AS) {
                Token *asToken = tokens.take();
                Token *openParen = tokens.peek();
                if (openParen->type != TokenType::OPEN_PAREN) {
                    throw std::runtime_error("Expected '(' after 'as' but got: " +
                                             std::string(tokens.peek()->text));
                }
                tokens.take();
                Type *targetType = parse_type(tokens);
                if (tokens.peek()->type != TokenType::CLOSE_PAREN) {
                    throw std::runtime_error("Expected ')' after type but got: " +
                                             std::string(tokens.peek()->text));
                }
                tokens.take(); // consume close paren
                expr = ast_arena_new<CastExpression>(expr, dot, asToken, openParen, targetType, tokens.take());
            } else {
                throw std::runtime_error("Expected identifier or operator after '.' but got: " +
                                         std::string(tokens.peek()->text));
            }
        }

        return expr;
    }

    Expression *parse_unary_expression(TokenIterator &tokens) {
        if (tokens.peek()->type == TokenType::NOT) {
            Token *op = tokens.take();
            Expression *operand = parse_unary_expression(tokens);
            return ast_arena_new<UnaryPrefixExpression>(op, operand);
        } else {
            return parse_dot_expression(tokens);
        }
    }

    constexpr int get_binary_precedence(TokenType op) {
        switch (op) {
        case TokenType::IDENTIFIER:
        case TokenType::INTEGER:
        case TokenType::STRING:
        case TokenType::OPEN_PAREN:
        case TokenType::NOT:
        case TokenType::DOT:
            return 0;
        case TokenType::STAR:
        case TokenType::SLASH:
            return 1;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 2;
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return 3;
        case TokenType::EQUAL_EQUAL:
        case TokenType::NOT_EQUAL:
            return 4;
        default:
            return 5;
        }
    }

    Expression *parse_bin_expression(TokenIterator &tokens,
                                     int precedence = get_binary_precedence(TokenType::EQUAL_EQUAL),
                                     std::vector<TokenType> stopTokens = {}) {
        if (precedence < 1) {
            return parse_unary_expression(tokens);
        }

        Expression *left = parse_bin_expression(tokens, precedence - 1, stopTokens);

        while (true) {
            Token *op = tokens.peek();
            if (std::find(stopTokens.begin(), stopTokens.end(), op->type) != stopTokens.end()) {
                break;
            }

            if (get_binary_precedence(op->type) > precedence) {
                break;
            }

            // valid binary operator
            tokens.take();
            Expression *right = parse_bin_expression(tokens, precedence - 1, stopTokens);
            left = ast_arena_new<BinaryExpression>(left, op, right);
        }

        return left;
    }

    Expression *parse_expression(TokenIterator &tokens) { return parse_bin_expression(tokens); }

    Statement *parse_if_statement(TokenIterator &tokens) {
        assert(tokens.peek()->type == TokenType::IF);
        Token *ifToken = tokens.take(); // consume 'if'
        if (tokens.peek()->type != TokenType::OPEN_PAREN) {
            throw std::runtime_error("Expected '(' after 'if' but got: " +
                                     std::string(tokens.peek()->text));
        }
        tokens.take(); // consume '('
        Expression *condition = parse_expression(tokens);
        if (tokens.peek()->type != TokenType::CLOSE_PAREN) {
            throw std::runtime_error("Expected ')' after 'if' condition but got: " +
                                     std::string(tokens.peek()->text));
        }
        tokens.take(); // consume ')'

        Statement *thenBranch = parse_statement(tokens);

        Statement *elseBranch = nullptr;
        if (tokens.peek()->type == TokenType::ELSE) {
            tokens.take(); // consume 'else'
            elseBranch = parse_statement(tokens);
        }
        return ast_arena_new<IfStatement>(ifToken, condition, thenBranch, elseBranch);
    }

    Statement *parse_let_statement(TokenIterator &tokens) {
        // TODO support initializer, multi-variable declarations, etc
        assert(tokens.peek()->type == TokenType::LET);
        Token *letToken = tokens.take(); // consume 'let'
        if (tokens.peek()->type != TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier after 'let' but got: " +
                                     std::string(tokens.peek()->text));
        }
        Token *name = tokens.take(); // consume identifier

        Type *type = nullptr;
        if (tokens.peek()->type == TokenType::COLON) {
            tokens.take(); // consume ':'
            if (tokens.peek()->type != TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected type after 'let <name>:' but got: " +
                                         std::string(tokens.peek()->text));
            }
            type = parse_type(tokens);
        }

        if (tokens.peek()->type != TokenType::SEMICOLON) {
            throw std::runtime_error("Expected ';' after 'let' declaration but got: " +
                                     std::string(tokens.peek()->text));
        }
        tokens.take(); // consume ';'

        return ast_arena_new<LetStatement>(letToken, name, type);
    }

    Statement *parse_return_statement(TokenIterator &tokens) {
        assert(tokens.peek()->type == TokenType::RET);
        Token *returnToken = tokens.take(); // consume 'ret'
        Expression *value = parse_expression(tokens);
        if (tokens.peek()->type != TokenType::SEMICOLON) {
            throw std::runtime_error("Expected ';' after 'ret' statement but got: " +
                                     std::string(tokens.peek()->text));
        }
        tokens.take(); // consume ';'
        return ast_arena_new<ReturnStatement>(returnToken, value);
    }

    BlockStatement *parse_block_statement(TokenIterator &tokens) {
        assert(tokens.peek()->type == TokenType::OPEN_BRACE);
        Token *openBrace = tokens.take(); // consume '{'
        std::vector<Statement *> statements;
        while (tokens.peek()->type != TokenType::CLOSE_BRACE) {
            statements.push_back(parse_statement(tokens));
        }
        Token *closeBrace = tokens.take(); // consume '}'
        return ast_arena_new<BlockStatement>(openBrace, statements, closeBrace);
    }

    Statement *parse_statement(TokenIterator &tokens) {
        // todo: while, for, switch, etc
        if (tokens.peek()->type == TokenType::IF) {
            return parse_if_statement(tokens);
        } else if (tokens.peek()->type == TokenType::LET) {
            return parse_let_statement(tokens);
        } else if (tokens.peek()->type == TokenType::RET) {
            return parse_return_statement(tokens);
        } else if (tokens.peek()->type == TokenType::OPEN_BRACE) {
            return parse_block_statement(tokens);
        } else {
            // default to expression statement
            Expression *expr = parse_expression(tokens);
            if (tokens.peek()->type != TokenType::SEMICOLON) {
                throw std::runtime_error("Expected ';' after expression statement but got: " +
                                         std::string(tokens.peek()->text));
            }
            Token *semicolon = tokens.take(); // consume ';'
            return ast_arena_new<ExpressionStatement>(expr->begin(), expr, semicolon);
        }
    }

    ArgList *parse_arg_list(TokenIterator &tokens) {
        assert(tokens.peek()->type == TokenType::OPEN_PAREN);
        Token *openParen = tokens.take(); // consume '('
        std::vector<Argument *> args;
        while (tokens.peek()->type != TokenType::CLOSE_PAREN) {
            if (tokens.peek()->type != TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected argument name but got: " +
                                         std::string(tokens.peek()->text));
            }
            Token *argName = tokens.take(); // consume argument name
            if (tokens.peek()->type != TokenType::COLON) {
                throw std::runtime_error("Expected ':' after argument name but got: " +
                                         std::string(tokens.peek()->text));
            }
            tokens.take(); // consume ':'
            Type *argType = parse_type(tokens);
            args.push_back(ast_arena_new<Argument>(argName, argType));

            if (tokens.peek()->type == TokenType::COMMA) {
                tokens.take(); // consume ','
            } else if (tokens.peek()->type == TokenType::CLOSE_PAREN) {
                break;
            } else {
                throw std::runtime_error("Expected ',' or ')' in argument list but got: " +
                                         std::string(tokens.peek()->text));
            }
        }
        Token *closeParen = tokens.take(); // consume ')'
        return ast_arena_new<ArgList>(openParen, args, closeParen);
    }

    Declaration *parse_declaration(TokenIterator &tokens) {
        if (tokens.peek()->type == TokenType::IMPORT) {
            Token *importToken = tokens.take(); // consume 'import'
            if (tokens.peek()->type != TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected identifier after 'import' but got: " +
                                         std::string(tokens.peek()->text));
            }
            Token *path = tokens.take(); // consume string literal
            if (tokens.peek()->type != TokenType::SEMICOLON) {
                throw std::runtime_error("Expected ';' after import declaration but got: " +
                                         std::string(tokens.peek()->text));
            }
            Token *semicolon = tokens.take(); // consume ';'
            return ast_arena_new<ImportDeclaration>(importToken, path, semicolon);
        } else if (tokens.peek()->type == TokenType::FUN) {
            Token *funToken = tokens.take(); // consume 'fun'
            if (tokens.peek()->type != TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected identifier after 'fun' but got: " +
                                         std::string(tokens.peek()->text));
            }
            Token *name = tokens.take(); // consume function name
            if (tokens.peek()->type != TokenType::OPEN_PAREN) {
                // TODO: support generics
                throw std::runtime_error("Expected '(' after function name but got: " +
                                         std::string(tokens.peek()->text));
            }

            ArgList *args = parse_arg_list(tokens);

            Token *returnArrow = nullptr;
            Type *returnType = nullptr;
            if (tokens.peek()->type == TokenType::ARROW) {
                returnArrow = tokens.take(); // consume '->'
                returnType = parse_type(tokens);
            }

            if (tokens.peek()->type == TokenType::SEMICOLON) {
                // no return type, just a declaration
                return ast_arena_new<FunctionDeclaration>(funToken,
                                               name,
                                               args,
                                               returnArrow,
                                               returnType,
                                               tokens.take());
            } else if (tokens.peek()->type == TokenType::OPEN_BRACE) {
                auto body = parse_block_statement(tokens);
                return ast_arena_new<FunctionDefinition>(funToken, name, args, returnArrow, returnType, body);
            } else {
                throw std::runtime_error(
                    "Expected ';' or '{' after function declaration but got: " +
                    std::string(tokens.peek()->text));
            }
        } else {
            throw std::runtime_error("Expected declaration but got: " +
                                     std::string(tokens.peek()->text));
        }
    }

    Declaration *parse(std::string_view input) {
        rena_arena_init(&ast_arena, 4096, 0);

        TokenIterator tokens(input);
        Declaration *d = parse_declaration(tokens);
        if (tokens.peek()->type != TokenType::END_OF_INPUT) {
            throw std::runtime_error("Unexpected token at end of input: " +
                                     std::string(tokens.peek()->text));
        }
        return d;
    }

} // namespace arena::parse