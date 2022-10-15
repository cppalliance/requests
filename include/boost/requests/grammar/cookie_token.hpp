// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_GRAMMAR_TOKEN_HPP
#define BOOST_REQUESTS_COOKIES_GRAMMAR_TOKEN_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/charset.hpp>

namespace boost {
namespace requests {
namespace grammar
{

/** The set of a token

    @par Example
    Character sets are used with rules and the
    functions @ref find_if and @ref find_if_not.
    @code
    result< string_view > rv = parse( "2122", token_rule( cookie_octet ) );
    @endcode

    @par BNF
    @code
       token          = 1*<any CHAR except CTLs or separators>
       separators     = "(" | ")" | "<" | ">" | "@"
                      | "," | ";" | ":" | "\" | <">
                      | "/" | "[" | "]" | "?" | "="
                      | "{" | "}" | SP | HT
       CTL            = <any US-ASCII control character
                        (octets 0 - 31) and DEL (127)>
       SP             = <US-ASCII SP, space (32)>
       HT             = <US-ASCII HT, horizontal-tab (9)>
    @endcode

    @par Specification
    @li <a href="https://www.rfc-editor.org/rfc/rfc2616#section-2.2"
        >2.2 Basic Rules (rfc2616)</a>

    @see
        @ref find_if,
        @ref find_if_not,
        @ref parse,
        @ref token_rule.
*/
#ifdef BOOST_URL_DOCS
constexpr __implementation_defined__ token;
#else

struct cookie_token_t
{
    constexpr
    bool
    operator()(char c) const noexcept
    {
        // NOT CTL & space
        return c > '\x20'
               && c != '\x7F'
               && c != '(' && c != ')' && c != '<' && c != '>' && c != '@'
               && c != ',' && c != ';' && c != ':' && c != '\\' && c != '"'
               && c != '/' && c != '[' && c != ']' && c != '?' && c != '='
               && c != '{' && c != '}';;
    }

#ifdef BOOST_URL_USE_SSE2

    char const *
    find_if(
            char const *first,
            char const *last) const noexcept
    {
        return urls::grammar::detail::find_if_pred(
                *this, first, last);
    }

    char const *
    find_if_not(
            char const *first,
            char const *last) const noexcept
    {
        return urls::grammar::detail::find_if_not_pred(
                *this, first, last);
    }

#endif
};

constexpr cookie_token_t cookie_token{};
#endif

} // grammar
} // requests
} // boost


#endif //BOOST_REQUESTS_COOKIES_GRAMMAR_TOKEN_HPP
