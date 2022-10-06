// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SANE_COOKIE_DATE_HPP
#define BOOST_REQUESTS_SANE_COOKIE_DATE_HPP

#include <chrono>
#include <boost/url/error_types.hpp>

namespace boost {
namespace requests {
namespace cookies {
namespace grammar {


/** The date used in Expires setting value

    @par Example
    @code
    result< std::chrono::time_point<std::chrono::system_clock> > rv =
        parse( "Sun, 06 Nov 1994 08:49:37 GMT", path_av );
    @endcode

    @par BNF
    @code
       rfc1123-date = wkday "," SP date1 SP time SP "GMT"
       date1        = 2DIGIT SP month SP 4DIGIT
              ; day month year (e.g., 02 Jun 1982)
       time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
              ; 00:00:00 - 23:59:59
       wkday        = "Mon" | "Tue" | "Wed"
                    | "Thu" | "Fri" | "Sat" | "Sun"
       month        = "Jan" | "Feb" | "Mar" | "Apr"
                    | "May" | "Jun" | "Jul" | "Aug"
                    | "Sep" | "Oct" | "Nov" | "Dec"
    @endcode

    @par Specification
    @li <a href="https://www.rfc-editor.org/rfc/rfc2616#section-3.3.1"
        >3.3.1 Full Date(rfc2616)</a>
*/
#ifdef BOOST_URL_DOCS
constexpr __implementation_defined__ non_zero_digit;
#else
struct sane_cookie_date_t
{
    using value_type = std::chrono::system_clock::time_point;

    BOOST_URL_DECL auto
    parse(
            char const*& it,
            char const* end
    ) const noexcept ->
    urls::error_types::result<value_type>;

};

constexpr sane_cookie_date_t sane_cookie_date{};
#endif


}
}
}
}


#endif //BOOST_REQUESTS_SANE_COOKIE_DATE_HPP
