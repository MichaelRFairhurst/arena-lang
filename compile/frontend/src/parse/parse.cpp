#include <string>
#include <string_view>
#include <algorithm>
#include <utility>
#include <optional>
#include <iostream>
#include <cassert>

#include "ast/token.hpp"
#include "parse/parse.hpp"

namespace arena::parse {
    using namespace arena::ast;

    class TokenIterator {
        util::Arena *arena;
        std::string_view state;
        Token *prev = nullptr;
        Token *head = nullptr;

        Token *set_head(TokenType type, std::string_view text) {
            prev = head;
            head = arena->alloc<Token>(Token{type, text, nullptr});

            if (prev != nullptr) {
                prev->next = head;
            }

            return head;
        }

    public:
        TokenIterator(util::Arena *arena, std::string_view input) : arena(arena), state(input) {}

        Token *peek() {
            if (head == nullptr) {
                next();
            }
            return head;
        }

        Token *take_synthetic(TokenType type) {
            Token *synthetic = arena->alloc<Token>(type, std::string_view{""}, head);
            if (prev != nullptr) {
                prev->next = synthetic;
            }
            return synthetic;
        }

        Token *take() {
            if (head == nullptr) {
                next();
            }
            next();
            return prev;
        }

        Token *next() {
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
            state = std::string_view(alphaNumEnd, state.end() - alphaNumEnd);

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

    class Parser {
    public:
        Parser(TokenIterator tokens, util::Arena *arena)
            : tokens(std::move(tokens)), arena(arena) {}

        static constexpr int get_binary_precedence(TokenType op) {
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

        std::vector<TypeArgument *> parse_generic_args() {
            auto openToken = require_take_token(TokenType::LESS, "to start generic parameter list");
            std::vector<TypeArgument *> generic_args;
            while (true) {
                require_consume_until(
                    {
                        TokenType::STAR,
                        TokenType::IDENTIFIER,
                        TokenType::GREATER,
                        TokenType::OPEN_PAREN,
                        TokenType::OPEN_BRACE,
                        TokenType::SEMICOLON,
                    },
                    "Expected a type argument (starting with identifier or '*') in generic "
                    "parameter list");

                if (tokens.peek()->type == TokenType::GREATER) {
                    auto closeToken = tokens.take(); // consume '>'
                    if (generic_args.empty()) {
                        errors.report(closeToken,
                                      closeToken,
                                      "Generic parameter list cannot be empty");
                    }

                    break;
                } else if (tokens.peek()->type == TokenType::OPEN_PAREN ||
                           tokens.peek()->type == TokenType::OPEN_BRACE ||
                           tokens.peek()->type == TokenType::SEMICOLON) {
                    errors.report(tokens.peek(),
                                  tokens.peek(),
                                  "Expected a type argument in generic parameter list but got: " +
                                      std::string(tokens.peek()->text));
                    break;
                }

                if (!generic_args.empty()) {
                    Token *comma =
                        require_take_token(TokenType::COMMA,
                                           "to separate type arguments in generic parameter list");
                }

                if (tokens.peek()->type == TokenType::STAR) {
                    Token *asterisk = tokens.take(); // consume '*'
                    Token *lifetime = require_take_token(TokenType::IDENTIFIER,
                                                         "to specify lifetime name after '*' in "
                                                         "generic parameter list");
                    generic_args.push_back(arena->alloc<TypeArgumentLifetime>(asterisk, lifetime));
                } else {
                    Type *given_type = parse_type();
                    generic_args.push_back(
                        arena->alloc<TypeArgumentType>(given_type->begin(), given_type));
                }
            }

            return generic_args;
        }

        Type *parse_type() {
            // TODO: const types
            Type *type;
            Token *name = require_take_token(TokenType::IDENTIFIER, "to start type");

            if (tokens.peek()->type == TokenType::LESS) {
                std::vector<TypeArgument *> generic_args = parse_generic_args();
                type = arena->alloc<NamedType>(name, generic_args);
            } else {
                type = arena->alloc<NamedType>(name, std::vector<TypeArgument *>{});
            }

            do {
                if (tokens.peek()->type == TokenType::STAR) {
                    Token *asterisk = tokens.take();
                    if (tokens.peek()->type == TokenType::IDENTIFIER) {
                        Token *lifetime = tokens.take();
                        type = arena->alloc<PointerType>(asterisk, type, lifetime);
                    } else {
                        type = arena->alloc<PointerType>(asterisk, type, nullptr);
                    }
                } else if (tokens.peek()->type == TokenType::OPEN_BRACKET) {
                    Token *openBracket = tokens.take();
                    Literal *size = parse_literal();
                    Token *closeBracket =
                        require_take_token(TokenType::CLOSE_BRACKET, "to close array type");
                    type = arena->alloc<ArrayType>(type, openBracket, size, closeBracket);
                } else {
                    break;
                }
            } while (true);

            return type;
        }

        Literal *parse_literal() {
            // TODO: actually parse different literals
            auto literalToken = require_take_token(TokenType::IDENTIFIER, "to start literal");
            return arena->alloc<Literal>(literalToken);
        }

        Expression *parse_primary_expression() {
            if (tokens.peek()->type == TokenType::OPEN_PAREN) {
                Token *open_paren = tokens.take();
                Expression *expr = parse_expression();
                Token *close_paren =
                    require_take_token(TokenType::CLOSE_PAREN, "to close parenthesized expression");
                return expr;
            }

            auto idToken =
                require_take_token(TokenType::IDENTIFIER, "to start identifier expression");
            return arena->alloc<IdExpression>(idToken);
        }

        Expression *parse_dot_expression(Expression *prev_expr) {
            Token *dot =
                require_take_token(TokenType::DOT, "to start member access or method call");
            if (tokens.peek()->type == TokenType::STAR || tokens.peek()->type == TokenType::AMP) {
                Token *op = tokens.take();
                return arena->alloc<DotOperatorExpression>(prev_expr, dot, op);
            } else if (tokens.peek()->type == TokenType::AS) {
                Token *asToken = tokens.take();
                Token *openParen =
                    require_take_token(TokenType::OPEN_PAREN, "after 'as' in cast expression");
                Type *targetType = parse_type();
                Token *closeParen =
                    require_take_token(TokenType::CLOSE_PAREN, "after type in cast expression");
                return arena->alloc<CastExpression>(prev_expr,
                                                    dot,
                                                    asToken,
                                                    openParen,
                                                    targetType,
                                                    closeParen);
            }

            Token *member =
                require_take_token(TokenType::IDENTIFIER, "after '.' in member access expression");
            return arena->alloc<MemberAccessExpression>(prev_expr, dot, member);
        }

        Expression *parse_call_expression(Expression *prev_expr) {
            Token *openParen = require_take_token(TokenType::OPEN_PAREN, "to start function call");
            Token *closeParen = nullptr;
            std::vector<Expression *> args;
            bool arg_allowed = true;
            while (true) {
                if (tokens.peek()->type == TokenType::CLOSE_PAREN) {
                    closeParen = tokens.take();
                    break;
                }

                if (!arg_allowed) {
                    errors.report(tokens.peek(),
                                  tokens.peek(),
                                  "Expected ')' or ',' but got: " +
                                      std::string(tokens.peek()->text));

                    if (tokens.peek()->type == TokenType::END_OF_INPUT ||
                        tokens.peek()->type == TokenType::SEMICOLON ||
                        tokens.peek()->type == TokenType::CLOSE_BRACE ||
                        tokens.peek()->type == TokenType::CLOSE_PAREN) {
                        break;
                    }
                }

                args.push_back(parse_expression());

                // Parse optional comma (may be trailing comma or separator)
                if (tokens.peek()->type == TokenType::COMMA) {
                    arg_allowed = true;
                    tokens.take();
                } else {
                    // No comma, so next token must be close paren.
                    arg_allowed = false;
                }
            }

            return arena->alloc<CallExpression>(prev_expr, openParen, args, closeParen);
        }

        Expression *parse_postfix_expression() {
            Expression *expr = parse_primary_expression();

            while (true) {
                if (tokens.peek()->type == TokenType::DOT) {
                    expr = parse_dot_expression(expr);
                } else if (tokens.peek()->type == TokenType::OPEN_PAREN) {
                    expr = parse_call_expression(expr);
                } else {
                    break;
                }
            }

            return expr;
        }

        Expression *parse_unary_expression() {
            if (tokens.peek()->type == TokenType::NOT) {
                Token *op = tokens.take();
                Expression *operand = parse_unary_expression();
                return arena->alloc<UnaryPrefixExpression>(op, operand);
            } else {
                return parse_postfix_expression();
            }
        }

        Expression *parse_bin_expression(
            int precedence = get_binary_precedence(TokenType::EQUAL_EQUAL)) {
            if (precedence < 1) {
                return parse_unary_expression();
            }

            Expression *left = parse_bin_expression(precedence - 1);

            while (true) {
                Token *op = tokens.peek();

                if (get_binary_precedence(op->type) > precedence) {
                    break;
                }

                // valid binary operator
                tokens.take();
                Expression *right = parse_bin_expression(precedence - 1);
                left = arena->alloc<BinaryExpression>(left, op, right);
            }

            return left;
        }

        Expression *parse_assignment_expression() {
            Expression *left = parse_bin_expression();
            if (tokens.peek()->type == TokenType::EQUAL) {
                Token *op = tokens.take();
                Expression *right = parse_assignment_expression();
                return arena->alloc<BinaryExpression>(left, op, right);
            } else {
                return left;
            }
        }

        Expression *parse_expression() { return parse_assignment_expression(); }

        Statement *parse_if_statement() {
            Token *if_token = require_take_token(TokenType::IF, "to start if statement");
            require_take_token(TokenType::OPEN_PAREN, "after 'if'");
            Expression *condition = parse_expression();
            Token *closeParen = require_take_token(TokenType::CLOSE_PAREN, "after 'if' condition");

            Statement *then_branch = parse_statement();

            Statement *else_branch = nullptr;
            if (tokens.peek()->type == TokenType::ELSE) {
                tokens.take(); // consume 'else'
                else_branch = parse_statement();
            }
            return arena->alloc<IfStatement>(if_token, condition, then_branch, else_branch);
        }

        Statement *parse_let_statement() {
            // TODO multi-variable declarations, etc
            Token *letToken = require_take_token(TokenType::LET, "to start let declaration");
            Token *name = require_take_token(TokenType::IDENTIFIER, "for variable name");

            Type *type = nullptr;
            if (tokens.peek()->type == TokenType::COLON) {
                tokens.take(); // consume ':'
                type = parse_type();
            }

            Token *equalToken = nullptr;
            Expression *initializer = nullptr;
            if (tokens.peek()->type == TokenType::EQUAL) {
                equalToken = tokens.take(); // consume '='
                initializer = parse_expression();
            }


            Token *semicolon = require_take_token(TokenType::SEMICOLON, "after let declaration");

            return arena
                ->alloc<LetStatement>(letToken, name, type, equalToken, initializer, semicolon);
        }

        Statement *parse_return_statement() {
            assert(tokens.peek()->type == TokenType::RET);
            Token *returnToken = require_take_token(TokenType::RET, "to start return statement");
            Expression *value = parse_expression();
            Token *semicolon = require_take_token(TokenType::SEMICOLON, "after return statement");
            return arena->alloc<ReturnStatement>(returnToken, value);
        }

        BlockStatement *parse_block_statement() {
            Token *openBrace =
                require_take_token(TokenType::OPEN_BRACE, "to start block statement");
            std::vector<Statement *> statements;
            while (tokens.peek()->type != TokenType::CLOSE_BRACE) {
                statements.push_back(parse_statement());
            }
            Token *closeBrace =
                require_take_token(TokenType::CLOSE_BRACE, "to end block statement");
            return arena->alloc<BlockStatement>(openBrace, statements, closeBrace);
        }

        Statement *parse_statement() {
            // todo: while, for, switch, etc
            if (tokens.peek()->type == TokenType::IF) {
                return parse_if_statement();
            } else if (tokens.peek()->type == TokenType::LET) {
                return parse_let_statement();
            } else if (tokens.peek()->type == TokenType::RET) {
                return parse_return_statement();
            } else if (tokens.peek()->type == TokenType::OPEN_BRACE) {
                return parse_block_statement();
            } else {
                // default to expression statement
                Expression *expr = parse_expression();
                Token *semicolon =
                    require_take_token(TokenType::SEMICOLON, "after expression statement");
                return arena->alloc<ExpressionStatement>(expr->begin(), expr, semicolon);
            }
        }

        ParamList *parse_param_list() {
            Token *openParen = require_take_token(TokenType::OPEN_PAREN, "to start parameter list");
            std::vector<Parameter *> params;
            while (true) {

                require_consume_until({TokenType::IDENTIFIER,
                                       TokenType::CLOSE_PAREN,
                                       TokenType::CLOSE_BRACE,
                                       TokenType::OPEN_BRACE,
                                       TokenType::COLON,
                                       TokenType::SEMICOLON,
                                       TokenType::COMMA,
                                       TokenType::ARROW},
                                      "Expected parameter name or ')' in parameter list");

                if (tokens.peek()->type == TokenType::CLOSE_PAREN ||
                    tokens.peek()->type == TokenType::ARROW ||
                    tokens.peek()->type == TokenType::CLOSE_BRACE ||
                    tokens.peek()->type == TokenType::OPEN_BRACE ||
                    tokens.peek()->type == TokenType::SEMICOLON) {
                    break;
                }

                if (params.size() > 0) {
                    require_take_token(TokenType::COMMA, "between parameters in parameter list");
                }

                Token *paramName = require_take_token(TokenType::IDENTIFIER, "as parameter name");
                Token *colon = require_take_token(TokenType::COLON, "after parameter name");

                Type *paramType = parse_type();
                params.push_back(arena->alloc<Parameter>(paramName, paramType));
            }
            Token *closeParen = require_take_token(TokenType::CLOSE_PAREN, "to end parameter list");
            return arena->alloc<ParamList>(openParen, params, closeParen);
        }

        FunctionDeclaration *parse_function() {
            Token *funToken = require_take_token(TokenType::FUN, "to start function declaration");
            Token *name = require_take_token(TokenType::IDENTIFIER, "after 'fun' keyword");

            ParamList *params = parse_param_list();

            Token *returnArrow = nullptr;
            Type *returnType = nullptr;
            require_consume_until({TokenType::SEMICOLON,
                                   TokenType::OPEN_BRACE,
                                   TokenType::CLOSE_BRACE,
                                   TokenType::ARROW},
                                  "Expected '->', '{', or ';' after function parameter list");

            if (tokens.peek()->type == TokenType::ARROW) {
                returnArrow = tokens.take(); // consume '->'
                returnType = parse_type();
            }

            if (tokens.peek()->type == TokenType::SEMICOLON) {
                // no return type, just a declaration
                return arena->alloc<FunctionDeclaration>(funToken,
                                                         name,
                                                         params,
                                                         returnArrow,
                                                         returnType,
                                                         tokens.take() // consume ';'
                );
            }

            auto body = parse_block_statement();
            return arena
                ->alloc<FunctionDefinition>(funToken, name, params, returnArrow, returnType, body);
        }

        StructDeclaration *parse_struct() {
            Token *structToken = tokens.take();
            assert(structToken->type == TokenType::STRUCT);
            Token *name = require_take_token(TokenType::IDENTIFIER, "after 'struct' keyword");

            if (tokens.peek()->type == TokenType::SEMICOLON) {
                Token *semicolon = tokens.take();
                return arena->alloc<StructDeclaration>(structToken, name, semicolon);
            }

            Token *openBrace =
                require_take_token(TokenType::OPEN_BRACE, "or ';' after struct name");

            std::vector<Field *> fields;
            while (true) {
                if (tokens.peek()->type == TokenType::CLOSE_BRACE ||
                    tokens.peek()->type == TokenType::END_OF_INPUT) {
                    break;
                }

                Token *name = tokens.take();
                if (name->type != TokenType::IDENTIFIER) {
                    error_until({TokenType::IDENTIFIER,
                                 TokenType::SEMICOLON,
                                 TokenType::CLOSE_BRACE},
                                "Expected field name in struct declaration but got: " +
                                    std::string(name->text));
                    continue;
                }

                Token *colon = require_take_token(TokenType::COLON, "after field name");
                Type *type = parse_type();
                Token *semicolon =
                    require_take_token(TokenType::SEMICOLON, "after field declaration");

                fields.push_back(arena->alloc<Field>(name, colon, type, semicolon));
            }

            Token *closeBrace =
                require_take_token(TokenType::CLOSE_BRACE, "to close struct declaration");

            return arena->alloc<StructDefinition>(structToken, name, openBrace, fields, closeBrace);
        }

        Declaration *parse_declaration() {
            if (tokens.peek()->type == TokenType::IMPORT) {
                Token *importToken = tokens.take(); // consume 'import'
                Token *path = require_take_token(TokenType::IDENTIFIER, "after 'import' keyword");
                Token *semicolon =
                    require_take_token(TokenType::SEMICOLON, "after import declaration");
                return arena->alloc<ImportDeclaration>(importToken, path, semicolon);
            } else if (tokens.peek()->type == TokenType::FUN) {
                return parse_function();
            } else if (tokens.peek()->type == TokenType::STRUCT) {
                return parse_struct();
            } else {
                error_until({TokenType::IMPORT,
                             TokenType::FUN,
                             TokenType::STRUCT,
                             TokenType::CLOSE_BRACE},
                            "Expected declaration but got: " + std::string(tokens.peek()->text));
                return nullptr;
            }
        }

        Token *require_take_token(TokenType expected, std::string_view context_message) {
            if (tokens.peek()->type == expected) {
                return tokens.take();
            } else {
                std::string context_message_spaced =
                    context_message.empty() ? "" : " " + std::string(context_message) + " ";
                std::string message = "Expected " + std::string(token_type_to_string(expected)) +
                                      context_message_spaced + " but got '" +
                                      std::string(tokens.peek()->text) + "'";
                errors.report(tokens.peek(), tokens.peek(), message);
                return tokens.take_synthetic(expected);
            }
        }

        void require_consume_until(std::vector<TokenType> expected, std::string_view message) {
            if (std::find(expected.begin(), expected.end(), tokens.peek()->type) !=
                expected.end()) {
                return;
            }

            error_until(expected, message);
        }

        void error_until(std::vector<TokenType> stopTokens, std::string_view message) {
            Token *begin = tokens.peek();
            Token *end = tokens.take();
            while (std::find(stopTokens.begin(), stopTokens.end(), tokens.peek()->type) ==
                   stopTokens.end()) {
                end = tokens.take();
                if (tokens.peek()->type == TokenType::END_OF_INPUT) {
                    break;
                }
            }

            errors.report(begin, end, std::string(message));
        }

        void parse(ParseResult &result) {
            do {
                auto decl = parse_declaration();
                if (decl != nullptr) {
                    result.declarations.push_back(decl);
                }
            } while (tokens.peek()->type != TokenType::END_OF_INPUT);
            result.errors = errors.get_errors();
        }

    private:
        util::Arena *arena;
        TokenIterator tokens;
        error::Reporter errors;
    };

    ParseResult parse(std::string_view input) {
        ParseResult result;
        TokenIterator tokens(&result.ast_arena, input);
        Parser parser(tokens, &result.ast_arena);
        parser.parse(result);

        return result;
    }

} // namespace arena::parse