// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_GRAMMAR_IMPL_SANE_COOKIE_DATE_IPP
#define BOOST_REQUESTS_COOKIES_GRAMMAR_IMPL_SANE_COOKIE_DATE_IPP

#include <boost/requests/cookies/grammar/fixed_token_rule.hpp>
#include <boost/requests/cookies/grammar/sane_cookie_date.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>

namespace boost {
namespace requests {
namespace cookies {
namespace grammar {

auto
    sane_cookie_date_t::parse(
            char const*& it,
            char const* end
    ) const noexcept ->
    urls::error_types::result<value_type>
{
    namespace ug = boost::urls::grammar;
    constexpr auto wkday = ug::variant_rule(
            // epoch was a thursday
            ug::literal_rule("Thu"),
            ug::literal_rule("Fri"),
            ug::literal_rule("Sat"),
            ug::literal_rule("Sun"), // moved here for the index -> sunday is the first day of the week
            ug::literal_rule("Mon"),
            ug::literal_rule("Tue"),
            ug::literal_rule("Wed")
    );
    constexpr auto month =  ug::variant_rule(
            ug::literal_rule("Jan"),
            ug::literal_rule("Feb"),
            ug::literal_rule("Mar"),
            ug::literal_rule("Apr"),
            ug::literal_rule("May"),
            ug::literal_rule("Jun"),
            ug::literal_rule("Jul"),
            ug::literal_rule("Aug"),
            ug::literal_rule("Sep"),
            ug::literal_rule("Oct"),
            ug::literal_rule("Nov"),
            ug::literal_rule("Dec")
            );

    constexpr auto date1 =
            ug::tuple_rule(
                fixed_token_rule<2u>(ug::digit_chars),
                ug::squelch(ug::literal_rule(" ")),
                month,
                ug::squelch(ug::literal_rule(" ")),
                fixed_token_rule<4u>(ug::digit_chars)
            );


    constexpr auto time =
            ug::tuple_rule(
                    fixed_token_rule<2u>(ug::digit_chars),
                    ug::squelch(ug::literal_rule(":")),
                    fixed_token_rule<2u>(ug::digit_chars),
                    ug::squelch(ug::literal_rule(":")),
                    fixed_token_rule<2u>(ug::digit_chars)
            );

    auto res =
            ug::parse(it, end,
            ug::tuple_rule(
                wkday,
                ug::squelch(ug::literal_rule(", ")),
                date1,
                ug::squelch(ug::literal_rule(" ")),
                time,
                ug::squelch(ug::literal_rule(" GMT"))
                ));

    if (res.has_error())
        return res.error();

    const auto raw = res.value();

    std::chrono::system_clock::time_point ts{};
    const std::size_t wd = std::get<0>(raw).index();
    const auto d1 = std::get<1>(raw);
    const auto t1 = std::get<2>(raw);

    std::chrono::seconds time_{};
    time_ += std::chrono::seconds(std::stoi(std::get<2>(t1)));
    time_ += std::chrono::minutes(std::stoi(std::get<1>(t1)));
    time_ += std::chrono::hours(std::stoi(std::get<0>(t1)));

    const auto year = std::stoi(std::get<2>(d1));
    if (year < 1970)
    {
        BOOST_URL_RETURN_EC(urls::grammar::error::out_of_range);
    }

    switch (std::get<1>(d1).index())
    {
        case 11: // december
            time_ += std::chrono::hours(30 * 24);
            BOOST_FALLTHROUGH;
        case 10: // november
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 9: // october
            time_ += std::chrono::hours(30 * 24);
            BOOST_FALLTHROUGH;
        case 8: // september
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 7: // august
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 6: // july
            time_ += std::chrono::hours(30 * 24);
            BOOST_FALLTHROUGH;
        case 5: // june
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 4: // may
            time_ += std::chrono::hours(30 * 24);
            BOOST_FALLTHROUGH;
        case 3: // april
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 2: // march
            time_ += std::chrono::hours(28 * 24);
            if (year % 4 == 0)
                time_ += std::chrono::hours(24);
            BOOST_FALLTHROUGH;
        case 1: // february
            time_ += std::chrono::hours(31 * 24);
            BOOST_FALLTHROUGH;
        case 0: // january
            break;
    }

    const auto yd = year - 1970;
    time_ += std::chrono::hours((yd * 365 * 24)  + ((yd + 2) / 4) * 24 );
    time_ += std::chrono::hours((std::stoi(std::get<0>(d1)) - 1) * 24);

    const auto days = (std::chrono::duration_cast<std::chrono::hours>(time_).count() / 24u) % 7;

    if (wd != days)
    {
        BOOST_URL_RETURN_EC(urls::grammar::error::mismatch);
    }
    return std::chrono::system_clock::time_point{time_};

}

}
}
}
}

#endif //BOOST_REQUESTS_COOKIES_GRAMMAR_IMPL_SANE_COOKIE_DATE_IPP
