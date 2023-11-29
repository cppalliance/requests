//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <boost/requests/rfc/quoted_string.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/url/grammar/parse.hpp>


BOOST_AUTO_TEST_SUITE(rfc_quoted_string);

BOOST_AUTO_TEST_CASE(quoted_string)
{
  namespace br = boost::requests;
  namespace ug = boost::urls::grammar;

  BOOST_CHECK(ug::parse("\"foobar\"", br::rfc::quoted_string) == "\"foobar\"");
  BOOST_CHECK(ug::parse("foobar", br::rfc::quoted_string) == ug::error::mismatch);
  BOOST_CHECK(ug::parse("\"foo\\\"bar\"", br::rfc::quoted_string) == "\"foo\\\"bar\"");
  BOOST_CHECK(br::rfc::unquote_string(ug::parse("\"foo\\\"bar\"", br::rfc::quoted_string).value())== "foo\"bar");
}

BOOST_AUTO_TEST_SUITE_END();
