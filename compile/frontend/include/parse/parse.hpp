#ifndef INCLUDE_PARSE_PARSE_HPP
#define INCLUDE_PARSE_PARSE_HPP

#include "ast/declarations.hpp"

namespace arena::parse {
    ast::Declaration *parse(std::string_view input);
}

#endif