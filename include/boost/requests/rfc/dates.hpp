//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_DATES_HPP
#define BOOST_REQUESTS_RFC_DATES_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>

namespace boost {
namespace requests {
namespace rfc {


/** The date according to rfc1123

    @par Example
    @code
    result< std::chrono::time_point<std::chrono::system_clock> > rv =
        parse( "Sun, 06 Nov 1994 08:49:37 GMT", date_1123 );
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
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ date_1123;
#else
struct date_1123_t
{
    using value_type = std::chrono::system_clock::time_point;

    BOOST_REQUESTS_DECL auto
    parse(
            char const*& it,
            char const* end
    ) const noexcept ->
    urls::error_types::result<value_type>;

};

constexpr date_1123_t date_1123{};
#endif

/// The date as used by cookies
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ sane_cookie_date{};
#else
constexpr date_1123_t sane_cookie_date{};
#endif

/** The date according to rfc1123

    @par Example
    @code
    result< std::chrono::time_point<std::chrono::system_clock> > rv =
        parse( "Sunday, 06-Nov-94 08:49:37 GMT", path_av );
    @endcode

    @par BNF
    @code
       rfc850-date    = weekday "," SP date2 SP time SP "GMT"
       date2          = 2DIGIT "-" month "-" 2DIGIT
              ; day-month-year (e.g., 02-Jun-82)
       time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
              ; 00:00:00 - 23:59:59
       weekday        = "Monday" | "Tuesday" | "Wednesday"
                      | "Thursday" | "Friday" | "Saturday" | "Sunday"
       month        = "Jan" | "Feb" | "Mar" | "Apr"
                    | "May" | "Jun" | "Jul" | "Aug"
                    | "Sep" | "Oct" | "Nov" | "Dec"
    @endcode

    @par Specification
    @li <a href="https://www.w3.org/Protocols/HTTP/1.1/draft-ietf-http-v11-spec-01#HTTP-Date"
        >3.3.1 Full Date</a>
*/
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ date_1123;
#else
struct date_850_t
{
  using value_type = std::chrono::system_clock::time_point;

  BOOST_REQUESTS_DECL auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      urls::error_types::result<value_type>;

};

constexpr date_850_t date_850{};
#endif


/** The date according to      asctime-date

    @par Example
    @code
    result< std::chrono::time_point<std::chrono::system_clock> > rv =
        parse( "Sun Nov  6 08:49:37 1994", path_av );
    @endcode

    @par BNF
    @code
       asctime-date   = wkday SP date3 SP time SP 4DIGIT
       date3          = month SP ( 2DIGIT | ( SP 1DIGIT ))
                 ; month day (e.g., Jun  2)
       time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                 ; 00:00:00 - 23:59:59
       wkday        = "Mon" | "Tue" | "Wed"
                    | "Thu" | "Fri" | "Sat" | "Sun"
       month        = "Jan" | "Feb" | "Mar" | "Apr"
                    | "May" | "Jun" | "Jul" | "Aug"
                    | "Sep" | "Oct" | "Nov" | "Dec"
    @endcode

    @par Specification
    @li <a href="https://www.w3.org/Protocols/HTTP/1.1/draft-ietf-http-v11-spec-01#HTTP-Date"
        >3.3.1 Full Date</a>
*/
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ date_asctime;
#else
struct date_asctime_t
{
  using value_type = std::chrono::system_clock::time_point;

  BOOST_REQUESTS_DECL auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      urls::error_types::result<value_type>;

};

constexpr date_asctime_t date_asctime{};
#endif

#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ http_date;
#else
struct http_date_t
{
  using value_type = std::chrono::system_clock::time_point;

  BOOST_REQUESTS_DECL auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      urls::error_types::result<value_type>;

};

constexpr http_date_t http_date;
#endif

}
}
}

#endif //BOOST_REQUESTS_RFC_DATES_HPP
