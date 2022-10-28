//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/fields/location.hpp>
#include <boost/url/parse.hpp>
#include <vector>
#include "doctest.h"
#include "string_maker.hpp"

#include <iostream>

TEST_SUITE_BEGIN("fields");

inline auto operator""_url( const char * ur, std::size_t sz)
{
  boost::core::string_view sv{ur, sz};
  return boost::urls::parse_uri_reference(sv);
}

TEST_CASE("location")
{
  using boost::requests::interpret_location;

  CHECK(interpret_location("/api/user", "/api/team")                == "/api/team"_url);
  CHECK(interpret_location("/api/user", "../group")                 == "/api/group"_url);
  CHECK(interpret_location("/api/user", "https://foo.com/api/team") == "https://foo.com/api/team"_url);
  CHECK(interpret_location("/api/user", "avatar")                   == "/api/user/avatar"_url);
  CHECK(interpret_location("/api/user#bio", "avatar#frag")          == "/api/user/avatar#frag"_url);
  CHECK(interpret_location("/api/user#bio", "avatar")               == "/api/user/avatar#bio"_url);
  CHECK(interpret_location("/api#user", "https://foo.com/api/team") == "https://foo.com/api/team#user"_url);
}

TEST_SUITE_END();