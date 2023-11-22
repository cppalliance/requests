//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/grammar/token_rule.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/alnum_chars.hpp>

BOOST_AUTO_TEST_SUITE(grammar);

BOOST_AUTO_TEST_CASE(token_rule)
{
  using namespace boost;
  constexpr auto tk = requests::grammar::token_rule(
            urls::grammar::alpha_chars,
            urls::grammar::alnum_chars
          );

  BOOST_CHECK(urls::grammar::error::mismatch == urls::grammar::parse("1234", tk));
  BOOST_CHECK("a123" == urls::grammar::parse("a123", tk));
}

BOOST_AUTO_TEST_SUITE_END();