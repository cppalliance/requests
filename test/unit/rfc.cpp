//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/rfc/dates.hpp>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(rfc);

using namespace boost;

BOOST_AUTO_TEST_CASE(date_1123)
{
  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sun, 06 Nov 1994 08:49:37 GMT", requests::rfc::date_1123));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::date_1123));
  
  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tue, 03 Oct 2023 12:11:15 GMT", requests::rfc::date_1123));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sun, 03 Oct 2023 12:11:15 GMT", requests::rfc::date_1123));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wed, 09 Jun 2021 10:18:14 GMT", requests::rfc::date_1123));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
             urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::date_1123));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::date_1123));

}

BOOST_AUTO_TEST_CASE(date_850)
{
  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sunday, 06-Nov-1994 08:49:37 GMT", requests::rfc::date_850));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::date_850));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tuesday, 03-Oct-2023 12:11:15 GMT", requests::rfc::date_850));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sunday, 03-Oct-2023 12:11:15 GMT", requests::rfc::date_850));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wednesday, 09-Jun-2021 10:18:14 GMT", requests::rfc::date_850));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thursday, 01-Jan-1970 00:00:01 GMT",  requests::rfc::date_850));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
            urls::grammar::parse("Thursday, 01-Jan-1970 00:00:01 GMT",  requests::rfc::date_850));

}



BOOST_AUTO_TEST_CASE(date_asctime)
{
  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sun Nov  6 08:49:37 1994", requests::rfc::date_asctime));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::date_asctime));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tue Oct 03 12:11:15 2023", requests::rfc::date_asctime));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sun Oct 03 12:11:15 2023", requests::rfc::date_asctime));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wed Jun  9 10:18:14 2021", requests::rfc::date_asctime));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu Jan  1 00:00:01 1970",  requests::rfc::date_asctime));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu Jan 01 00:00:01 1970",  requests::rfc::date_asctime));

}


BOOST_AUTO_TEST_CASE(http_date)
{
  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sun, 06 Nov 1994 08:49:37 GMT", requests::rfc::http_date));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tue, 03 Oct 2023 12:11:15 GMT", requests::rfc::http_date));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sun, 03 Oct 2023 12:11:15 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wed, 09 Jun 2021 10:18:14 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::http_date));


  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sunday, 06-Nov-1994 08:49:37 GMT", requests::rfc::http_date));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tuesday, 03-Oct-2023 12:11:15 GMT", requests::rfc::http_date));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sunday, 03-Oct-2023 12:11:15 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wednesday, 09-Jun-2021 10:18:14 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thursday, 01-Jan-1970 00:00:01 GMT",  requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thursday, 01-Jan-1970 00:00:01 GMT",  requests::rfc::http_date));


  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
              urls::grammar::parse("Sun Nov  6 08:49:37 1994", requests::rfc::http_date));
  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
              urls::grammar::parse("Tue Oct 03 12:11:15 2023", requests::rfc::http_date));

  BOOST_CHECK(urls::grammar::error::mismatch ==
              urls::grammar::parse("Sun Oct 03 12:11:15 2023", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
              urls::grammar::parse("Wed Jun  9 10:18:14 2021", requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu Jan  1 00:00:01 1970",  requests::rfc::http_date));

  BOOST_CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
              urls::grammar::parse("Thu Jan 01 00:00:01 1970",  requests::rfc::http_date));

}



BOOST_AUTO_TEST_SUITE_END();