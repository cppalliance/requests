//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_GRAMMAR_MIME_CHAR_HPP
#define BOOST_REQUESTS_GRAMMAR_MIME_CHAR_HPP

#include <boost/url/grammar/alnum_chars.hpp>


namespace boost {
namespace requests {
namespace grammar {

/** The set of ptoken cahrs

    @par Example
    Character sets are used with rules and the
    functions @ref find_if and @ref find_if_not.
    @code
    result< string_view > rv = parse( "UTF-8", token_rule( mime_char ) );
    @endcode

    @par BNF
    @code
      mime-charsetc = ALPHA / DIGIT
                    / "!" / "#" / "$" / "%" / "&"
                    / "+" / "-" / "^" / "_" / "`"
                    / "{" / "}" / "~"
                    ; as <mime-charset> in Section 2.3 of [RFC2978]
                    ; except that the single quote is not included
                    ; SHOULD be registered in the IANA charset registry
    @endcode

    @par Specification
    @li <a href="https://www.rfc-editor.org/rfc/rfc5987#section-3.2.1">3.2.1. Definition</a>

    @see
        @ref find_if,
        @ref find_if_not,
        @ref parse,
        @ref token_rule.
*/
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ mime_char;
#else

struct mime_char_t
{
  constexpr
      bool
      operator()(char c) const noexcept
  {
    return urls::grammar::alnum_chars(c)
           || c == '!'
           || c == '#'
           || c == '$'
           || c == '%'
           || c == '&'
           || c == '+'
           || c == '-'
           || c == '^'
           || c == '_'
           || c == '`'
           || c == '{'
           || c == '}'
           || c == '~'
        ;
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

constexpr mime_char_t mime_char{};
#endif

} // grammar
} // requests
} // boost


#endif // BOOST_REQUESTS_GRAMMAR_MIME_CHAR_HPP
