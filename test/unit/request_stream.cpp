//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/connection.hpp>
#include <boost/requests/request.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/json.hpp>
#include <boost/test/unit_test.hpp>

#include "../coroutine_test_case.hpp"
#include "../fixtures/server.hpp"

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
using boost::system::error_code ;

BOOST_FIXTURE_TEST_SUITE(request_stream, test_server);

BOOST_AUTO_TEST_CASE(request_connection)
{
  namespace http = boost::beast::http;

  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  auto res = requests::request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {
            /*.headers = */ {{"test", "dummy"}},
      });

  auto & s = res.first;
  auto & h = res.second;

  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK_EQUAL(s.read(dbuf), 12);
  BOOST_CHECK(str == "Hello World!");
  BOOST_CHECK(s.headers().at("test") == "dummy");

  BOOST_CHECK(h.size() == 3);
}


BOOST_AUTO_TEST_CASE(request_connection_too_many_redirects)
{
  namespace http = boost::beast::http;

  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  boost::system::error_code ec;

  auto res = requests::request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {
          /*.headers = */ {{"test", "dummy"}},
       {true, requests::redirect_mode::private_domain, 2}
      }, ec);

  auto & h = res.second;
  BOOST_CHECK(ec == requests::error::too_many_redirects);
  BOOST_CHECK(h.size() == 2);
}

BOOST_AUTO_TEST_CASE(request_connection_forbidden_redirect)
{
  namespace http = boost::beast::http;

  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  boost::system::error_code ec;

  auto res = requests::request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}},
       {true, requests::redirect_mode::none, 12}
      }, ec);

  auto & s = res.first;
  auto & h = res.second;
  BOOST_CHECK_MESSAGE(ec == requests::error::forbidden_redirect, ec.what());
  BOOST_CHECK(h.size() == 1);
}

BOOST_AUTO_TEST_CASE(request_connection_invalid_redirect)
{
  namespace http = boost::beast::http;

  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  boost::system::error_code ec;

  auto res = requests::request_stream(
      conn,
      http::verb::get,
      "/invalid-redirect",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}}}, ec);

  auto & s = res.first;
  auto & h = res.second;
  BOOST_CHECK_MESSAGE(ec == requests::error::invalid_redirect, ec.what());
  BOOST_CHECK(h.size() == 1);
}


BOOST_AUTO_TEST_CASE(request_connection_offsite)
{
  namespace http = boost::beast::http;

  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  boost::system::error_code ec;

  auto res = requests::request_stream(
      conn,
      http::verb::get,
      "/boost-redirect",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}}}, ec);

  auto & s = res.first;
  auto & h = res.second;
  BOOST_CHECK_MESSAGE(ec == requests::error::forbidden_redirect, ec.what());
  BOOST_CHECK(h.size() == 1);
}


BOOST_COROUTINE_TEST_CASE(async_request_connection)
{
  namespace http = boost::beast::http;

  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  auto res = requests::async_request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {
          /*.headers = */ {{"test", "dummy"}},
      }, yield);

  auto & s = std::get<0>(res);
  auto & h = std::get<1>(res);

  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK_EQUAL(s.async_read(dbuf, yield), 12);
  BOOST_CHECK(str == "Hello World!");
  BOOST_CHECK(s.headers().at("test") == "dummy");

  BOOST_CHECK(h.size() == 3);
}


BOOST_COROUTINE_TEST_CASE(async_request_connection_too_many_redirects)
{
  namespace http = boost::beast::http;

  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  boost::system::error_code ec;

  auto res = requests::async_request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}},
       {true, requests::redirect_mode::private_domain, 2}
      }, yield[ec]);

  auto & h = std::get<1>(res);
  BOOST_CHECK(ec == requests::error::too_many_redirects);
  BOOST_CHECK(h.size() == 2);
}

BOOST_COROUTINE_TEST_CASE(async_request_connection_forbidden_redirect)
{
  namespace http = boost::beast::http;

  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  boost::system::error_code ec;

  auto res = requests::async_request_stream(
      conn,
      http::verb::get,
      "/redirect/3",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}},
       {true, requests::redirect_mode::none, 12}
      },  yield[ec]);

  auto & s = std::get<0>(res);
  auto & h = std::get<1>(res);

  BOOST_CHECK_MESSAGE(ec == requests::error::forbidden_redirect, ec.what());
  BOOST_CHECK_EQUAL(h.size(), 1);
}

BOOST_COROUTINE_TEST_CASE(async_request_connection_invalid_redirect)
{
  namespace http = boost::beast::http;

  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  boost::system::error_code ec;

  auto res = requests::async_request_stream(
      conn,
      http::verb::get,
      "/invalid-redirect",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}}}, yield[ec]);

  auto & s = std::get<0>(res);
  auto & h = std::get<1>(res);

  BOOST_CHECK_MESSAGE(ec == requests::error::invalid_redirect, ec.what());
  BOOST_CHECK(h.size() == 1);
}


BOOST_COROUTINE_TEST_CASE(async_request_connection_offsite)
{
  namespace http = boost::beast::http;

  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  boost::system::error_code ec;

  auto res = requests::async_request_stream(
      conn,
      http::verb::get,
      "/boost-redirect",
      "test-data",
      {/*.headers = */ {{"test", "dummy"}}}, yield[ec]);

  auto & s = std::get<0>(res);
  auto & h = std::get<1>(res);

  BOOST_CHECK_MESSAGE(ec == requests::error::forbidden_redirect, ec.what());
  BOOST_CHECK(h.size() == 1);
}

// pool & session will be tested in the httpbin tests.

BOOST_AUTO_TEST_SUITE_END();
