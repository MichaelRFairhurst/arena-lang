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
            while (true) {
                auto begin = std::find_if_not(state.begin(), state.end(), isspace);

                state = std::string_view(begin, state.end() - begin);
                if (state.empty()) {
                    return set_head(TokenType::END_OF_INPUT, state);
                }

                if (state[0] != '/') {
                    break;
                }

                if (!lex_comment()) {
                    break;
                }
            }

            switch (state[0]) {
            case '+':
                return n_char_token(TokenType::PLUS, 1);
            case '-':
                return lex_match("->", TokenType::ARROW, [this]() {
                    return n_char_token(TokenType::MINUS, 1);
                });
            case '*':
                return n_char_token(TokenType::STAR, 1);
            case '/':
                return n_char_token(TokenType::SLASH, 1);
            case '=':
                return lex_match("==", TokenType::EQUAL_EQUAL, [this]() {
                    return n_char_token(TokenType::EQUAL, 1);
                });
            case '(':
                return n_char_token(TokenType::OPEN_PAREN, 1);
            case ')':
                return n_char_token(TokenType::CLOSE_PAREN, 1);
            case '[':
                return n_char_token(TokenType::OPEN_BRACKET, 1);
            case ']':
                return n_char_token(TokenType::CLOSE_BRACKET, 1);
            case '<':
                return lex_match("<=", TokenType::LESS_EQUAL, [this]() {
                    return n_char_token(TokenType::LESS, 1);
                });
            case '>':
                return lex_match(">=", TokenType::GREATER_EQUAL, [this]() {
                    return n_char_token(TokenType::GREATER, 1);
                });
            case '.':
                return n_char_token(TokenType::DOT, 1);
            case '!':
                return lex_match("!=", TokenType::NOT_EQUAL, [this]() {
                    return n_char_token(TokenType::NOT, 1);
                });
            case '&':
                return lex_match("&&", TokenType::AND, [this]() {
                    return n_char_token(TokenType::AMP, 1);
                });
            case '|':
                return lex_match("||", TokenType::OR, [this]() {
                    return n_char_token(TokenType::INVALID, 1);
                });
            case '{':
                return n_char_token(TokenType::OPEN_BRACE, 1);
            case '}':
                return n_char_token(TokenType::CLOSE_BRACE, 1);
            case ':':
                return n_char_token(TokenType::COLON, 1);
            case ';':
                return n_char_token(TokenType::SEMICOLON, 1);
            case ',':
                return n_char_token(TokenType::COMMA, 1);
            case 'a':
                return keyword_match("as", TokenType::AS, lex_ident());
            case 'c':
                return keyword_match("const", TokenType::CONST, lex_ident());
            case 'e':
                return keyword_match("else", TokenType::ELSE, lex_ident());
            case 'f':
                return keyword_match("fun",
                                     TokenType::FUN,
                                     keyword_match("false", TokenType::FALSE, lex_ident()));
            case 'i':
                return keyword_match("import",
                                     TokenType::IMPORT,
                                     keyword_match("if", TokenType::IF, lex_ident()));
            case 'l':
                return keyword_match("let", TokenType::LET, lex_ident());
            case 'r':
                return keyword_match("ret", TokenType::RET, lex_ident());
            case 's':
                return keyword_match("struct", TokenType::STRUCT, lex_ident());
            case 't':
                return keyword_match("true", TokenType::TRUE, lex_ident());
            case 'w':
                return keyword_match("while", TokenType::WHILE, lex_ident());
            case '"':
            case '\'':
                return lex_quoted_string();
            default:
                // fall through
                ;
            }

            if (std::isalpha(state[0]) || state[0] == '_') {
                return lex_ident();
            }

            if (std::isdigit(state[0])) {
                return lex_number();
            }

            return n_char_token(TokenType::INVALID, 1);
        }

    private:
        template <typename Else>
        Token *lex_match(std::string_view expected, TokenType type, Else &&elseFunc) {
            if (state.size() >= expected.size() && state.substr(0, expected.size()) == expected) {
                return n_char_token(type, expected.size());
            }

            return elseFunc();
        }

        Token *n_char_token(TokenType type, int n, Token::Value literalValue = {}) {
            std::string_view tokenText = state.substr(0, n);
            state.remove_prefix(n);
            return set_head(type, tokenText, literalValue);
        }

        Token *iter_token(TokenType type,
                          std::string_view::iterator it,
                          Token::Value literalValue = {}) {
            return n_char_token(type, std::distance(state.begin(), it), literalValue);
        }

        Token *keyword_match(std::string_view expected, TokenType type, Token *ident) {
            if (ident->text == expected) {
                ident->type = type;
            }

            return ident;
        }

        bool lex_comment() {
            if (state[0] != '/' || state.size() < 2) {
                return false;
            }

            if (state[1] == '/') {
                // single-line comment, skip to end of line
                auto it = std::find(state.begin(), state.end(), '\n');
                iter_token(TokenType::COMMENT, it);
                return true;
            }

            auto end = lex_multiline_comment_nestable(state.begin());
            if (end.has_value()) {
                iter_token(TokenType::COMMENT, end.value());
                return true;
            }

            return false;
        }

        /**
         * Lexes a multiline comment, but supports nesting.
         *
         * This allows you to use slash-star comments to comment out blocks of code that already
         * contain slash-star comments.
         */
        std::optional<std::string_view::iterator> lex_multiline_comment_nestable(
            std::string_view::iterator begin) {
            auto it = begin;
            if (it == state.end() || *it != '/') {
                return std::nullopt;
            }
            it++;

            if (it == state.end() || *it != '*') {
                return std::nullopt;
            }
            it++;

            for (; it != state.end(); ++it) {
                if (*it == '/') {
                    it = lex_multiline_comment_nestable(it).value_or(it + 1);
                }

                if (*it == '*') {
                    it++;
                    if (it != state.end() && *it == '/') {
                        return it + 1;
                    }
                }
            }

            // Unclosed comment, return end of input
            return it;
        }

        Token *lex_ident() {
            auto it = state.begin();
            auto end = state.end();

            for (; it != end; ++it) {
                if (!std::isalnum(*it) && *it != '_') {
                    break;
                }
            }

            return iter_token(TokenType::IDENTIFIER, it);
        }

        Token *lex_quoted_string() {
            char quoteChar = state[0];
            auto it = state.begin() + 1;
            std::string value;

            for (; it != state.end(); ++it) {
                if (*it == '\\') {
                    // skip escaped character
                    ++it;
                    if (it == state.end()) {
                        break;
                    }
                    switch (*it) {
                    case 'n':
                        value += '\n';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    case 'r':
                        value += '\r';
                        break;
                    case '0':
                        value += '\0';
                        break;
                    case '\\':
                        value += '\\';
                        break;
                    }
                } else if (*it == quoteChar) {
                    ++it;
                    break;
                } else {
                    value += *it;
                }
            }

            // Ensure data is stored in the parse tree arena.
            char *interned = arena->alloc_array<char>(value.size());
            std::copy(value.begin(), value.end(), interned);

            return iter_token(TokenType::STRING, it, std::string_view(interned, value.size()));
        }

        Token *lex_number() {
            __int128_t value = 0;
            int base = 10;
            int decimal = -1;
            auto it = state.begin();

            if (state.size() >= 2 && state[0] == '0') {
                if (state[1] == 'x' || state[1] == 'X') {
                    base = 16;
                    it += 2;
                } else if (state[1] == 'b' || state[1] == 'B') {
                    base = 2;
                    it += 2;
                }
            } else if (state.size() >= 2 && state[0] == '8' && state[1] == 'x') {
                base = 8;
                it += 2;
            }

            for (; it != state.end(); ++it) {
                if (*it == '\'' || *it == '_') {
                    continue; // skip digit separators
                }

                if (*it == '.') {
                    if (decimal != -1) {
                        break; // second decimal point, stop parsing
                    }
                    decimal = 1;
                    continue;
                } else if (decimal != -1) {
                    decimal *= base;
                }

                bool valid_digit = false;
                switch (base) {
                case 2: {
                    if (*it != '0' && *it != '1') {
                        break;
                    }
                    valid_digit = true;
                    value = value * 2 + (*it - '0');
                    break;
                }
                case 8: {
                    if (*it < '0' || *it > '7') {
                        break;
                    }
                    valid_digit = true;
                    value = value * 8 + (*it - '0');
                    break;
                }
                case 10: {
                    if (!std::isdigit(*it)) {
                        break;
                    }
                    valid_digit = true;
                    value = value * 10 + (*it - '0');
                    break;
                }

                case 16: {
                    if (!std::isxdigit(*it)) {
                        break;
                    }
                    valid_digit = true;
                    int digit = std::isdigit(*it) ? (*it - '0') : (std::tolower(*it) - 'a' + 10);
                    value = value * 16 + digit;
                    break;
                }
                default:
                    throw std::runtime_error("Invalid base");
                }

                if (!valid_digit) {
                    break;
                }
            }

            Token::Value literalValue;
            if (decimal == -1) {
                literalValue = static_cast<int64_t>(value);
            } else {
                literalValue = static_cast<double>(value) / decimal;
            }

            return iter_token(TokenType::INTEGER, it, literalValue);
        }

        Token *set_head(TokenType type, std::string_view text, Token::Value literalValue = {}) {
            prev = head;
            head = arena->alloc<Token>(Token{type, text, nullptr, literalValue});

            if (prev != nullptr) {
                prev->next = head;
            }

            return head;
        }

        util::Arena *arena;
        std::string_view state;
        Token *prev = nullptr;
        Token *head = nullptr;
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
                        errors.E_P_UNEXP(closeToken, "non-empty generic parameter list", "empty");
                    }

                    break;
                } else if (tokens.peek()->type == TokenType::OPEN_PAREN ||
                           tokens.peek()->type == TokenType::OPEN_BRACE ||
                           tokens.peek()->type == TokenType::SEMICOLON) {
                    errors.E_P_UNEXP(tokens.peek(),
                                     "a type argument in generic parameter list",
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
                } else if (tokens.peek()->type == TokenType::CONST) {
                    Token *constToken = tokens.take();
                    type = arena->alloc<ConstType>(constToken, type);
                } else {
                    break;
                }
            } while (true);

            return type;
        }

        Literal *parse_literal() {
            Token *tok;
            if (tokens.peek()->type == TokenType::INTEGER ||
                tokens.peek()->type == TokenType::STRING) {
                tok = tokens.take();
            } else {
                tok = tokens.take_synthetic(TokenType::INTEGER);
            }

            return arena->alloc<Literal>(tok);
        }

        Expression *parse_primary_expression() {
            if (tokens.peek()->type == TokenType::OPEN_PAREN) {
                Token *open_paren = tokens.take();
                Expression *expr = parse_expression();
                Token *close_paren =
                    require_take_token(TokenType::CLOSE_PAREN, "to close parenthesized expression");
                return expr;
            } else if (tokens.peek()->type == TokenType::INTEGER ||
                       tokens.peek()->type == TokenType::STRING) {
                Literal *literal = parse_literal();
                return arena->alloc<LiteralExpression>(literal);
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
                    errors.E_P_UNEXP(tokens.peek(),
                                     "')' or ',' but got: " + std::string(tokens.peek()->text),
                                     "");

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

        ArenaStatement *parse_arena_statement() {
            Token *arenaToken =
                require_take_token(TokenType::IDENTIFIER, "to start arena statement");
            assert(arenaToken->text == "arena");
            BlockStatement *block = parse_block_statement();
            return arena->alloc<ArenaStatement>(arenaToken, block);
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
            } else if (tokens.peek()->type == TokenType::IDENTIFIER &&
                       tokens.peek()->text == "arena") {
                return parse_arena_statement();
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
                errors.E_P_UNEXP(tokens.peek(),
                                 token_type_to_string(expected),
                                 tokens.peek()->text);
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

            errors.E_P_UNEXP(error::Location{begin, end}, "", "");
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