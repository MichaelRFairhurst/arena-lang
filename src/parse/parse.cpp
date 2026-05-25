//#include <boost/parser/parser.hpp>
//#include "parse/parse.hpp"
//#include "ast/expressions.hpp"
//
//namespace bp = boost::parser;
//namespace ast = arena::ast;
//
//template <typename InputIt>
//std::string_view to_sv(InputIt begin, InputIt end) {
//    return std::string_view(&(*begin), static_cast<size_t>(end - begin));
//}
//
//template <typename Context>
//std::string_view to_sv(Context &ctx) {
//    return to_sv(bp::_where(ctx).begin(), bp::_where(ctx).end());
//}
//
//template <typename T>
//T *intern_old(T &&obj) {
//    return new T(std::forward<T>(obj)); // TODO: implement actual interning
//}
//
//template <typename T, typename Tuple>
//T *intern_from_tuple(Tuple &&args) {
//    // TODO: implement actual interning
//    return std::apply([](auto
//                             &&...elems) {
//                                return new T(std::forward<decltype(elems)>(elems)...); },
//                      std::forward<Tuple>(args));
//}
//
//template <typename T>
//auto intern() {
//    return [](auto ctx) {
//        if constexpr (std::is_constructible_v<T, decltype(_attr(ctx))>) {
//            return new T(_attr(ctx));
//        } else {
//            //static_assert(sizeof(T) && sizeof(decltype(_attr(ctx))) && false, "Cannot construct type T from the given attribute");
//            return intern_from_tuple<T>(_attr(ctx));
//        }
//    };
//}
//
//template <ast::TokenType TT>
//auto const mk_token = [](auto ctx) { return ast::Token{TT, to_sv(ctx.begin(), ctx.end())}; };
//
//auto const mk_ident = [](auto &ctx) {
//    _val(ctx) = ast::Token{ast::TokenType::IDENTIFIER, to_sv(ctx)};
//};
//
//bp::rule<struct identifier_r, ast::Token> const identifier = "an identifier";
//bp::rule<struct id_expression_r, ast::Expression *> const id_expression =
//    "an identifier expression";
//bp::rule<struct primary_expression_r, ast::Expression *> const primary_expression =
//    "a primary expression, such as an identifier or literal";
//bp::rule<struct dot_expression_r, ast::Expression *> const dot_expression = "a 'x.?' expression";
//bp::rule<struct unary_expression_r, ast::Expression *> const unary_expression = "a unary expression";
//bp::rule<struct mul_expression_r, ast::Expression *> const mul_expression = "a multiplication expression";
//bp::rule<struct add_expression_r, ast::Expression *> const add_expression = "an addition expression";
//bp::rule<struct rel_expression_r, ast::Expression *> const rel_expression = "a relational expression";
//bp::rule<struct eq_expression_r, ast::Expression *> const eq_expression = "an equality expression";
//bp::rule<struct and_expression_r, ast::Expression *> const and_expression = "a logical AND expression";
//bp::rule<struct or_expression_r, ast::Expression *> const or_expression = "a logical OR expression";
//bp::rule<struct expression_r, ast::Expression *> const expression = "an expression";
//
//auto const identifier_char = bp::lower | bp::upper | bp::digit | '_' | '#' | ':' | '@' | '.';
//auto const identifier_def = +identifier_char[mk_ident];
//
//template <ast::TokenType TT>
//auto const tok = [](auto p) { return bp::transform(mk_token<TT>)[bp::raw[bp::string(p)]]; };
//
//auto const plus = tok<ast::TokenType::PLUS>("+");
//auto const minus = tok<ast::TokenType::MINUS>("-");
//auto const star = tok<ast::TokenType::STAR>("*");
//auto const slash = tok<ast::TokenType::SLASH>("/");
//auto const equal = tok<ast::TokenType::EQUAL>("=");
//auto const equal_equal = tok<ast::TokenType::EQUAL_EQUAL>("==");
//auto const not_equal = tok<ast::TokenType::NOT_EQUAL>("!=");
//auto const less = tok<ast::TokenType::LESS>("<");
//auto const less_equal = tok<ast::TokenType::LESS_EQUAL>("<=");
//auto const greater = tok<ast::TokenType::GREATER>(">");
//auto const greater_equal = tok<ast::TokenType::GREATER_EQUAL>(">=");
//auto const and_ = tok<ast::TokenType::AND>("&&");
//auto const or_ = tok<ast::TokenType::OR>("||");
//auto const open_paren = tok<ast::TokenType::OPEN_PAREN>("(");
//auto const close_paren = tok<ast::TokenType::CLOSE_PAREN>(")");
//auto const dot = tok<ast::TokenType::DOT>(".");
//auto const not_ = tok<ast::TokenType::NOT>("!");
//auto const amp = tok<ast::TokenType::AMP>("&");
//auto const as_kw = tok<ast::TokenType::AS>("as");
//auto const fun_kw = tok<ast::TokenType::FUN>("fun");
//
//auto const mul_op = star | slash;
//auto const add_op = plus | minus;
//auto const rel_op = less | less_equal | greater | greater_equal;
//auto const eq_op = equal_equal | not_equal;
//auto const dot_op = star | amp;
//
//// todo <expr>.as(T*)
//auto const primary_expression_def = id_expression;
//auto const id_expression_def = identifier[intern<ast::IdExpression>()];
//auto const dot_expression_def = (dot_expression >> dot >> identifier)[intern<ast::MemberAccessExpression>()] | ((dot_expression >> dot) > dot_op)[intern<ast::DotOperatorExpression>()] | id_expression;
//auto const unary_expression_def = (not_ > unary_expression)[intern<ast::UnaryPrefixExpression>()] | dot_expression;
//auto const mul_expression_def = (mul_expression >> mul_op >> unary_expression)[intern<ast::BinaryExpression>()] | unary_expression;
//auto const add_expression_def = (add_expression >> add_op >> mul_expression)[intern<ast::BinaryExpression>()] | mul_expression;
//auto const rel_expression_def = (rel_expression >> rel_op >> add_expression)[intern<ast::BinaryExpression>()] | add_expression;
//auto const eq_expression_def = (rel_expression >> eq_op >> rel_expression)[intern<ast::BinaryExpression>()] | rel_expression;
//auto const and_expression_def = (and_expression >> and_ >> eq_expression)[intern<ast::BinaryExpression>()] | eq_expression;
//auto const or_expression_def = (or_expression >> or_ >> and_expression)[intern<ast::BinaryExpression2>()] | and_expression;
//
//auto const expression_def = or_expression;
//
//BOOST_PARSER_DEFINE_RULES(
//    identifier, expression, id_expression, dot_expression, unary_expression, mul_expression, add_expression, rel_expression, eq_expression, and_expression, or_expression, primary_expression);
//
//namespace parse = arena::parse;
//ast::Expression parse::parse(std::string_view input) {
//    auto expr = bp::parse(input, expression, bp::ws);
//    if (expr.has_value()) {
//        return *expr.value();
//    } else {
//        throw std::runtime_error("Failed to parse expression: " + std::string(input));
//    }
//}