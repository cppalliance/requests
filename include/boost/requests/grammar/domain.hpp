// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_GRAMMAR_DOMAIN_HPP
#define BOOST_REQUESTS_COOKIES_GRAMMAR_DOMAIN_HPP


#include <boost/url/grammar.hpp>
#include <boost/url/rfc/pchars.hpp>

namespace boost {
namespace requests {
namespace grammar {

/** Match a fixed-sized string of characters from a set

    If there is not enough input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    result< string_view > rv = parse( "foo.bar", domain );
    @endcode

    @par BNF
    As given by the rfc-1034
    @code
    <subdomain> ::= <label> | <subdomain> "." <label>

    <label> ::= <letter> [ [ <ldh-str> ] <let-dig> ]

    <ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>

    <let-dig-hyp> ::= <let-dig> | "-"

    <let-dig> ::= <letter> | <digit>

    <letter> ::= any one of the 52 alphabetic characters A through Z in
    upper case and a through z in lower case

    <digit> ::= any one of the ten digits 0 through 9
    @endcode

    Simplified
    @code
    subdomain ::= label *( "." label )
    label     ::= alpha [ *( alphanum | "-" )  alphanum ]
    @endcode

    Since the mini parsing library doesn't have a look-ahead, we'll just implement that ourselves.

    @code{.dot}
    digraph finite_state_machine {
        rankdir=LR;

        init -> labelN[label=alpha];
        labelN -> labelN[label=alphanum];
        labelN -> "labelN-1"[label="'-'"]
        "labelN-1" -> "labelN-1"[label="'-'"]
        "labelN-1" -> labelN[label=alphanum]

        labelN -> done [label=eoi]

        labelN -> subdomain[label="'.'"]
        subdomain -> labelN[label=alpha]
    }
    @endcode

    @param cs The character set to use

    @see
        @ref alpha_chars,
        @ref parse.
*/
#ifdef BOOST_REQUESTS_DOCS
constexpr
__implementation_defined__
domain;
#else
struct domain_t
{
    using value_type = core::string_view;

    auto
    parse(
            char const*& it,
            char const* end
    ) const noexcept -> system::result<value_type>
    {


        namespace ug = urls::grammar;

        // RFC 6265 allows ignoring leading `.`

        if (it != end && *it == '.')
            it++;

        auto it0 = it;
        // set to indicate until where we could validly consume data
        auto valid_until = it0;

        const auto eoi = [&]{return it == end;};
        if (it == end)
            return ug::error::need_more;


      // init
        if (!ug::alpha_chars(*it))
            return ug::error::invalid;
        else
            goto labelN;

        if (false) labelN:
        {
            valid_until = ++it ;
            if (eoi())
                goto done;
            else if (ug::alnum_chars(*it))
                goto labelN;
            else if (*it ==  '-')
                goto labelN_1;
            else if (*it ==  '.')
                goto subdomain;
        }

        if (false) labelN_1:
        {
            it ++;
            if (eoi())
                goto done;
            else if (*it == '-')
                goto labelN_1;
            else if (ug::alnum_chars(*it))
                goto labelN;
        }

          if (false) subdomain:
        {
            it ++;
            if (eoi())
                goto done;
            else if (ug::alpha_chars(*it))
                goto labelN;
        }
      done:
        it = valid_until ;
        return core::string_view(it0, it);
    }


    constexpr
    domain_t() = default;
};

constexpr
domain_t domain;
#endif

} // grammar
} // requests
} // boost

#endif //BOOST_REQUESTS_COOKIES_GRAMMAR_DOMAIN_HPP
