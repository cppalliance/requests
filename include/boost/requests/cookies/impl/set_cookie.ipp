    // Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SET_COOKIE_IPP
#define BOOST_REQUESTS_SET_COOKIE_IPP

#include <boost/requests/cookies/set_cookie.hpp>
#include <boost/requests/cookies/grammar/cookie_octet.hpp>
#include <boost/requests/cookies/grammar/token.hpp>
#include <boost/requests/cookies/grammar/any_char_except.hpp>
#include <boost/requests/cookies/grammar/domain.hpp>
#include <boost/requests/cookies/grammar/sane_cookie_date.hpp>

namespace boost {
namespace requests {
namespace cookies {

bool set_cookie::extensions_only::operator()(const core::string_view & ra) const
{
    namespace ug = urls::grammar ;
    return !ug::ci_is_equal(ra, "Secure")
           && !ug::ci_is_equal(ra, "HttpOnly")
           && !ug::ci_is_equal(ra.substr(0, 8), "Expires=")
           && !ug::ci_is_equal(ra.substr(0, 8), "Max-Age=")
           && !ug::ci_is_equal(ra.substr(0, 7), "Domain=")
           && !ug::ci_is_equal(ra.substr(0, 5), "Path=");
}


system::result<set_cookie> parse_set_cookie_field(core::string_view input)
{
    namespace ug = urls::grammar;
    auto res = urls::grammar::parse(
            input,
            ug::tuple_rule(
                    ug::token_rule(grammar::token),
                    ug::squelch(ug::literal_rule("=")),
                    ug::token_rule(grammar::cookie_octets),
                    ug::range_rule(
                        ug::tuple_rule(
                            ug::squelch(ug::literal_rule("; ")),
                            ug::token_rule(grammar::any_char_except)
                        )
                    )
                )
            );

    if (res.has_error())
        return res.error();

    const auto & value = res.value();
    set_cookie sc{
        std::get<0>(value),
        std::get<1>(value),
        std::get<2>(res.value())
    };

    for (auto ra : sc.attributes)
    {
        if (ug::ci_is_equal(ra, "Secure"))
            sc.secure = true;
        else if (ug::ci_is_equal(ra, "HttpOnly"))
            sc.http_only = true;
        else if (ug::ci_is_equal(ra.substr(0, 8), "Expires="))
        {
            auto ires = ug::parse(ra.substr(8), grammar::sane_cookie_date);
            if (ires.has_error())
                return ires.error();
            else
                sc.expires = ires.value();
        }
        else if (ug::ci_is_equal(ra.substr(0, 8), "Max-Age="))
        {
            auto ires = ug::parse(ra.substr(8), ug::token_rule(ug::digit_chars));
            if (ires.has_error())
                return ires.error();
            else
                sc.max_age = std::chrono::seconds(std::stoull(ires.value()));
        }
        else if (ug::ci_is_equal(ra.substr(0, 7), "Domain="))
        {
            auto ires = ug::parse(ra.substr(7), grammar::domain);
            if (ires.has_error())
                return ires.error();
            else
                sc.domain = ires.value();
        }
        else if (ug::ci_is_equal(ra.substr(0, 5), "Path="))
        {
            auto ires = ug::parse(ra.substr(5), ug::token_rule(grammar::any_char_except));
            if (ires.has_error())
                return ires.error();
            else
                sc.path = ires.value();
        }

    }

    return sc;
}


}
}
}
#endif //BOOST_REQUESTS_SET_COOKIE_IPP
