//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/requests/fields/keep_alive.hpp"
#include <boost/asio/ip/tcp.hpp>

#include "doctest.h"
#include "string_maker.hpp"

TEST_SUITE_BEGIN("keep-alive");

TEST_CASE("parse")
{
  namespace br = boost::requests;

  const auto epoch = std::chrono::system_clock::time_point::min();

  CHECK(br::parse_keep_alive_field("max=42", epoch).value().max == 42);
  CHECK(br::parse_keep_alive_field("max=42", epoch).value().timeout
        == std::chrono::system_clock::time_point::max());

  CHECK(br::parse_keep_alive_field("timeout=23", epoch).value().max
        == std::numeric_limits<std::size_t>::max());
  CHECK(br::parse_keep_alive_field("timeout=23", epoch).value().timeout
        == std::chrono::system_clock::time_point(std::chrono::seconds(23)));

  CHECK(br::parse_keep_alive_field("max=12, timeout=34", epoch).value().max == 12);
  CHECK(br::parse_keep_alive_field("max=12, timeout=34", epoch).value().timeout
        == std::chrono::system_clock::time_point(std::chrono::seconds(34)));

  CHECK(br::parse_keep_alive_field("timeout=12, max=34", epoch).value().max == 34);
  CHECK(br::parse_keep_alive_field("timeout=12, max=34", epoch).value().timeout
        == std::chrono::system_clock::time_point(std::chrono::seconds(12)));

}

TEST_SUITE_END();