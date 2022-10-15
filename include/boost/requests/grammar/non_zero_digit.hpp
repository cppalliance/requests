// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_NON_ZERO_DIGIT_HPP
#define BOOST_REQUESTS_NON_ZERO_DIGIT_HPP


#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/detail/charset.hpp>

namespace boost {
namespace requests {
namespace grammar
{

/** The set of non-zero decimal digits

    @par Example
    Character sets are used with rules and the
    functions @ref find_if and @ref find_if_not.
    @code
    result< string_view > rv = parse( "2122", token_rule( non_zero_digit_t ) );
    @endcode

    @par BNF
    @code
    DIGIT       = %x31-39
                ; 1-9
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

struct non_zero_digit_t
{
    constexpr
    bool
    operator()(char c) const noexcept
    {
        return c >= '1' && c <= '9';
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

constexpr non_zero_digit_t non_zero_digit{};
#endif

} // grammar
} // requests
} // boost


#endif //BOOST_REQUESTS_NON_ZERO_DIGIT_HPP
