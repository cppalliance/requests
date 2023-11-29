//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/fields/keep_alive.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(keep_alive);

BOOST_AUTO_TEST_CASE(parse)
{
  namespace br = boost::requests;

  const auto epoch = std::chrono::system_clock::time_point::min();

  BOOST_CHECK(br::parse_keep_alive_field("max=42", epoch).value().max == 42);
  BOOST_CHECK(br::parse_keep_alive_field("max=42", epoch).value().timeout
              == std::chrono::system_clock::time_point::max());

  BOOST_CHECK(br::parse_keep_alive_field("timeout=23", epoch).value().max
              == std::numeric_limits<std::size_t>::max());
  BOOST_CHECK(br::parse_keep_alive_field("timeout=23", epoch).value().timeout
              == std::chrono::system_clock::time_point(std::chrono::seconds(23)));

  BOOST_CHECK(br::parse_keep_alive_field("max=12, timeout=34", epoch).value().max == 12);
  BOOST_CHECK(br::parse_keep_alive_field("max=12, timeout=34", epoch).value().timeout
              == std::chrono::system_clock::time_point(std::chrono::seconds(34)));

  BOOST_CHECK(br::parse_keep_alive_field("timeout=12, max=34", epoch).value().max == 34);
  BOOST_CHECK(br::parse_keep_alive_field("timeout=12, max=34", epoch).value().timeout
              == std::chrono::system_clock::time_point(std::chrono::seconds(12)));

}

BOOST_AUTO_TEST_SUITE_END();
