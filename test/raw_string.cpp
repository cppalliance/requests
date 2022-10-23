//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/grammar/raw_string.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include "doctest.h"
#include "string_maker.hpp"

TEST_SUITE_BEGIN("grammar");

TEST_CASE("fixed-token-rule")
{
  namespace br = boost::requests;
  namespace ug = boost::urls::grammar;

  auto rl = ug::tuple_rule(ug::token_rule(ug::alpha_chars),
                           ug::squelch(ug::literal_rule("=")),
                           ug::token_rule(ug::digit_chars));

  CHECK(ug::parse("x=1234", rl).value() == std::make_tuple("x", "1234"));
  CHECK(ug::parse("x=1234", br::grammar::raw_string(rl)) == "x=1234");

}

TEST_SUITE_END();