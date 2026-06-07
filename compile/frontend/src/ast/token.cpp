    #include "ast/token.hpp"

    std::string_view arena::ast::token_type_to_string(TokenType type) {
        switch (type) {
        case TokenType::IDENTIFIER:
            return "an identifier";
        case TokenType::INTEGER:
            return "an integer literal";
        case TokenType::STRING:
            return "a string literal";
        case TokenType::FLOATING:
            return "a floating-point literal";
        case TokenType::TRUE:
            return "'true'";
        case TokenType::FALSE:
            return "'false'";
        case TokenType::OPEN_BRACE:
            return "'{'";
        case TokenType::CLOSE_BRACE:
            return "'}'";
        case TokenType::OPEN_PAREN:
            return "'('";
        case TokenType::CLOSE_PAREN:
            return "')'";
        case TokenType::OPEN_BRACKET:
            return "'['";
        case TokenType::CLOSE_BRACKET:
            return "']'";
        case TokenType::SEMICOLON:
            return "';'";
        case TokenType::COLON:
            return "':'";
        case TokenType::COMMA:
            return "','";
        case TokenType::PLUS:
            return "'+'";
        case TokenType::MINUS:
            return "'-'";
        case TokenType::STAR:
            return "'*'";
        case TokenType::SLASH:
            return "'/'";
        case TokenType::EQUAL:
            return "'='";
        case TokenType::EQUAL_EQUAL:
            return "'=='";
        case TokenType::NOT_EQUAL:
            return "'!='";
        case TokenType::LESS:
            return "'<'";
        case TokenType::LESS_EQUAL:
            return "'<='";
        case TokenType::GREATER:
            return "'>'";
        case TokenType::GREATER_EQUAL:
            return "'>='";
        case TokenType::AND:
            return "'&&'";
        case TokenType::OR:
            return "'||'";
        case TokenType::NOT:
            return "'!'";
        case TokenType::DOT:
            return "'.'";
        case TokenType::AMP:
            return "'&'";
        case TokenType::ARROW:
            return "'->'";
        case TokenType::AS:
            return "'as'";
        case TokenType::FUN:
            return "'fun'";
        case TokenType::IF:
            return "'if'";
        case TokenType::ELSE:
            return "'else'";
        case TokenType::LET:
            return "'let'";
        case TokenType::WHILE:
            return "'while'";
        case TokenType::FOR:
            return "'for'";
        case TokenType::RET:
            return "'ret'";
        case TokenType::IMPORT:
            return "'import'";
        case TokenType::STRUCT:
            return "'struct'";
        case TokenType::END_OF_INPUT:
            return "'end of input'";
        case TokenType::UNINITIALIZED_TOKEN:
        default:
            return "Error or uninitialized token";
        }
    };