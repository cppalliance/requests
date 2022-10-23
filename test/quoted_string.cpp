//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <boost/requests/rfc/quoted_string.hpp>
#include <boost/url/grammar/parse.hpp>

#include "doctest.h"
#include "string_maker.hpp"


TEST_SUITE_BEGIN("rfc");

TEST_CASE("quoted_string")
{
  namespace br = boost::requests;
  namespace ug = boost::urls::grammar;

  CHECK(ug::parse(R"("foobar")", br::rfc::quoted_string) == "\"foobar\"");
  CHECK(ug::parse(R"(foobar)", br::rfc::quoted_string) == ug::error::mismatch);

  CHECK(ug::parse(R"("foo\"bar")", br::rfc::quoted_string) == "\"foo\\\"bar\"");

  CHECK(
      br::rfc::unquote_string(ug::parse(R"("foo\"bar")", br::rfc::quoted_string).value())
        == "foo\"bar");

}

TEST_SUITE_END();
