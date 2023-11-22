// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/json.hpp>
#include <boost/requests/json.hpp>

#include <boost/test/unit_test.hpp>

#include "../fixtures/httpbin.hpp"
#include "../coroutine_test_case.hpp"


using namespace boost;

BOOST_FIXTURE_TEST_SUITE(cookie_, httpbin);

BOOST_AUTO_TEST_CASE(http_request_cookie_connection)
{
  auto hc = connect();

  requests::cookie_jar jar;
  auto res = requests::json::get(hc, "/cookies", {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  BOOST_CHECK(res.value.at("cookies").as_object().empty());
  BOOST_CHECK(jar.content.empty());

  res = requests::json::get(hc, "/cookies/set?cookie-1=foo", {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  BOOST_CHECK(res.value.at("cookies") == (json::object{{"cookie-1", "foo"}}));
  BOOST_REQUIRE(!jar.content.empty());
  auto citr = jar.content.begin();
  BOOST_CHECK(citr->value == "foo");
  BOOST_CHECK(citr->name == "cookie-1");
  BOOST_CHECK(citr->secure_only_flag == false);
  BOOST_CHECK(citr->path == "/");

  res = requests::json::get(hc, "/cookies/set/cookie-2/bar", {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  BOOST_CHECK(res.value.at("cookies") == (json::object{{"cookie-1", "foo"}, {"cookie-2", "bar"}}));
  BOOST_REQUIRE(jar.content.size() == 2u);
  citr = jar.content.begin();
  if (citr->name == "cookie-1")
  {
    BOOST_CHECK(citr->value == "foo");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
    citr ++ ;
    BOOST_CHECK(citr->value == "bar");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");

  }
  else
  {
    BOOST_CHECK(citr->value == "bar");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
    citr ++ ;
    BOOST_CHECK(citr->value == "foo");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
  }
  res = requests::json::get(hc, urls::url_view{"/cookies/delete?cookie-1"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  BOOST_CHECK(!jar.content.empty());
  BOOST_REQUIRE(jar.content.size() == 1u);
  citr = jar.content.begin();
  BOOST_CHECK(citr->value == "bar");
  BOOST_CHECK(citr->name == "cookie-2");
  BOOST_CHECK(citr->secure_only_flag == false);
  BOOST_CHECK(citr->path == "/");

  res = requests::json::get(hc, urls::url_view{"/cookies/delete?cookie-2"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});
  BOOST_CHECK(jar.content.empty());
}

BOOST_COROUTINE_TEST_CASE(http_request_async_cookie_connection)
{
  auto hc = async_connect(yield);

  requests::cookie_jar jar;
  BOOST_CHECK(jar.content.empty());
  auto res = requests::json::async_get(hc, "/cookies", {{}, /*.opts=*/{false}, /*.jar=*/&jar}, {}, yield);

  BOOST_CHECK(res.value.at("cookies").as_object().empty());
  BOOST_CHECK(jar.content.empty());

  res = requests::json::async_get(hc, "/cookies/set?cookie-1=foo", {{}, /*.opts=*/{false}, /*.jar=*/&jar}, {}, yield);

  BOOST_CHECK(res.value.at("cookies") == (json::object{{"cookie-1", "foo"}}));
  BOOST_REQUIRE(!jar.content.empty());
  auto citr = jar.content.begin();
  BOOST_CHECK(citr->value == "foo");
  BOOST_CHECK(citr->name == "cookie-1");
  BOOST_CHECK(citr->secure_only_flag == false);
  BOOST_CHECK(citr->path == "/");

  res = requests::json::async_get(hc, "/cookies/set/cookie-2/bar", {{}, /*.opts=*/{false}, /*.jar=*/&jar}, {}, yield);

  BOOST_CHECK(res.value.at("cookies") == (json::object{{"cookie-1", "foo"}, {"cookie-2", "bar"}}));
  BOOST_REQUIRE(jar.content.size() == 2u);
  citr = jar.content.begin();
  if (citr->name == "cookie-1")
  {
    BOOST_CHECK(citr->value == "foo");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
    citr ++ ;
    BOOST_CHECK(citr->value == "bar");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");

  }
  else
  {
    BOOST_CHECK(citr->value == "bar");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
    citr ++ ;
    BOOST_CHECK(citr->value == "foo");
    BOOST_CHECK(citr->secure_only_flag == false);
    BOOST_CHECK(citr->path == "/");
  }
  res = requests::json::async_get(hc, urls::url_view{"/cookies/delete?cookie-1"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar}, {}, yield);

  BOOST_CHECK(!jar.content.empty());
  BOOST_REQUIRE(jar.content.size() == 1u);
  citr = jar.content.begin();
  BOOST_CHECK(citr->value == "bar");
  BOOST_CHECK(citr->name == "cookie-2");
  BOOST_CHECK(citr->secure_only_flag == false);
  BOOST_CHECK(citr->path == "/");

  res = requests::json::async_get(hc, urls::url_view{"/cookies/delete?cookie-2"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar}, {}, yield);
  BOOST_CHECK(jar.content.empty());
}


BOOST_AUTO_TEST_SUITE_END();