//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/fields/link.hpp>
#include <boost/url/parse.hpp>
#include <vector>

#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(rfc_link);

using namespace boost;

namespace br = boost::requests;

BOOST_AUTO_TEST_CASE(single)
{
  auto txt = R"(<https://one.example.com>)";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());
  BOOST_REQUIRE(lst->size() == 1u);
  BOOST_CHECK((*lst->begin()).attributes.empty());
  BOOST_CHECK((*lst->begin()).url == urls::parse_uri("https://one.example.com"));
}

BOOST_AUTO_TEST_CASE(multiple)
{
  auto txt = R"(<https://one.example.com>, <https://two.example.com>)";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());

  BOOST_REQUIRE(lst->size() == 2u);

  BOOST_CHECK((*lst->begin()).attributes.empty());
  BOOST_CHECK((*lst->begin()).url == urls::parse_uri("https://one.example.com").value());

  BOOST_CHECK((*std::next(lst->begin())).attributes.empty());
  BOOST_CHECK((*std::next(lst->begin())).url == urls::parse_uri("https://two.example.com").value());
}


BOOST_AUTO_TEST_CASE(single_with_param)
{
  auto txt = R"(<https://one.example.com>; foobar)";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());

  BOOST_CHECK(lst->size() == 1u);

  auto val = *lst->begin();
  BOOST_CHECK(val.attributes.begin() != val.attributes.end());

  std::vector<br::link::field> vec;
  vec.assign(val.attributes.begin(), val.attributes.end());
  BOOST_CHECK(vec.size() == 1u);
  if (!vec.empty())
  {
    BOOST_CHECK(val.url == urls::parse_uri("https://one.example.com"));
    BOOST_CHECK(vec.front().key == "foobar");
    BOOST_CHECK(vec.front().value.empty());
  }
}

BOOST_AUTO_TEST_CASE(single_with_param_eq)
{
  auto txt = R"(<https://one.example.com>; xyz=ctl)";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());

  BOOST_CHECK(lst->size() == 1u);
  auto  val = *lst->begin();
  BOOST_CHECK(val.attributes.begin() != val.attributes.end());

  std::vector<br::link::field> vec;
  vec.assign(val.attributes.begin(), val.attributes.end());
  BOOST_CHECK(vec.size() == 1u);
  if (!vec.empty())
  {
    BOOST_CHECK(val.url == urls::parse_uri("https://one.example.com"));
    BOOST_CHECK(vec.front().key == "xyz");
    BOOST_CHECK(vec.front().value == "ctl");
  }
}


BOOST_AUTO_TEST_CASE(single_with_param_rel)
{
  auto txt = R"(<https://one.example.com>; rel="preconnect")";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());


  BOOST_CHECK(lst->size() == 1u);
  auto  val = *lst->begin();
  BOOST_CHECK(val.attributes.begin() != val.attributes.end());

  auto rel = val.rel();
  BOOST_CHECK_MESSAGE(!rel.has_error(), rel.error());
  BOOST_CHECK(rel->size() == 1);
}

BOOST_AUTO_TEST_CASE(multiple_with_param)
{
  auto txt =
      R"(<https://one.example.com>; rel="preconnect next", )"
      R"(<https://two.example.com>; rel="preconnect", )"
      R"(<https://three.example.com>; rel="preconnect")";
  auto lst = br::parse_link_field(txt);
  BOOST_CHECK_MESSAGE(!lst.has_error(), lst.error());
}

BOOST_AUTO_TEST_SUITE_END();