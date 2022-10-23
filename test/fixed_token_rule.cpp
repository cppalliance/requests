//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/grammar/fixed_token_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/alpha_chars.hpp>

#include "doctest.h"
#include "string_maker.hpp"

TEST_SUITE_BEGIN("grammar");

TEST_CASE("fixed-token-rule")
{
  namespace br = boost::requests;
  namespace ug = boost::urls::grammar;

  auto tk1 = br::grammar::fixed_token_rule<3u>(ug::alpha_chars);
  CHECK(ug::parse("ab", tk1)   == ug::error::need_more);
  CHECK(ug::parse("abc", tk1)  == "abc");
  CHECK(ug::parse("abcd", tk1) == ug::error::leftover);


  auto tk2 = br::grammar::fixed_token_rule<3u, 5u>(ug::alpha_chars);
  CHECK(ug::parse("ab", tk2)     == ug::error::need_more);
  CHECK(ug::parse("abc", tk2)    == "abc");
  CHECK(ug::parse("abcd", tk2)   == "abcd");
  CHECK(ug::parse("abcde", tk2)  == "abcde");
  CHECK(ug::parse("abcdef", tk2) == ug::error::leftover);
}

TEST_SUITE_END();