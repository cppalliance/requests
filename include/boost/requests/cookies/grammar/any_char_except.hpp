// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_ANY_CHAR_EXCEPT_HPP
#define BOOST_REQUESTS_ANY_CHAR_EXCEPT_HPP



#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/detail/charset.hpp>

namespace boost {
namespace requests {
namespace cookies
{
namespace grammar
{

/** The set of any-char values in cookies

    @par Example
    Character sets are used with rules and the
    functions @ref find_if and @ref find_if_not.
    @code
    result< string_view > rv = parse( "foo:/bar", token_rule( non_zero_digit_t ) );
    @endcode

    @par BNF
    @code
    DIGIT       = <any CHAR except CTLs or ";">
    CTL         =  %x00-1F / %x7F
    @endcode

    @par Specification
    @li <a href="https://www.rfc-editor.org/rfc/rfc6265#section-4.1"
        >4.1.1.  Syntax  (rfc6265)</a>

    @see
        @ref find_if,
        @ref find_if_not,
        @ref parse,
        @ref token_rule.
*/
#ifdef BOOST_URL_DOCS
constexpr __implementation_defined__ non_zero_digit;
#else

struct any_char_except_t
{
    constexpr
    bool
    operator()(char c) const noexcept
    {
        return (c > '\x1F') && c != '\x7F' && c != ';';
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

constexpr any_char_except_t any_char_except{};
#endif

} // grammar
} // cookies
} // requests
} // boost



#endif //BOOST_REQUESTS_ANY_CHAR_EXCEPT_HPP
