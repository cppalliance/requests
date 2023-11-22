// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
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

BOOST_FIXTURE_TEST_SUITE(connection, test_server);

BOOST_AUTO_TEST_CASE(echo)
{
  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.ropen(http::verb::post, "/echo", hd, ss, nullptr);
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK(s.read(dbuf) == 9);
  BOOST_CHECK(str == "test-data");
  BOOST_CHECK(s.headers().at("test") == "dummy");

  conn.close();
}

BOOST_AUTO_TEST_CASE(echo_chunked)
{
  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.ropen(http::verb::post, "/echo-chunked", hd, ss, nullptr);
  BOOST_CHECK(s.headers().at(http::field::transfer_encoding) == "chunked");
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK_EQUAL(s.read(dbuf), 9);
  BOOST_CHECK_EQUAL(str.size(), 9);
  BOOST_CHECK(str == "test-data");
  BOOST_CHECK(s.headers().at("test") == "dummy");
  conn.close();
}

BOOST_AUTO_TEST_CASE(boost_get_http)
{
  asio::io_context ctx;

  requests::connection conn{ctx};
  asio::ip::tcp::resolver res{ctx};
  auto eps = res.resolve("boost.org", "http");
  conn.set_host("boost.org");
  conn.connect(eps.begin()->endpoint());

  BOOST_REQUIRE(!conn.uses_ssl());
  namespace http = boost::beast::http;

  requests::http::headers hd;
  auto src = requests::make_source(requests::empty{});
  conn.ropen(http::verb::get, "/", hd, *src, nullptr).dump();
  conn.close();
}

BOOST_AUTO_TEST_CASE(amazon_get_https)
{
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context_base::tlsv12_client};
  sslctx.set_default_verify_paths();
  sslctx.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);

  requests::connection conn{ctx, sslctx};
  asio::ip::tcp::resolver res{ctx};
  auto eps = res.resolve("amazon.com", "https");
  conn.set_host("amazon.com");
  conn.connect(eps.begin()->endpoint());

  BOOST_REQUIRE(conn.uses_ssl());
  namespace http = boost::beast::http;

  requests::http::headers hd;
  auto src = requests::make_source(requests::empty{});
  conn.ropen(http::verb::get, "/", hd, *src, nullptr).dump();
  conn.close();
}

BOOST_AUTO_TEST_CASE(amazon_get_https_invalid_host)
{
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context_base::tlsv12_client};
  sslctx.set_default_verify_paths();
  sslctx.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
  requests::connection conn{ctx, sslctx};
  asio::ip::tcp::resolver res{ctx};
  auto eps = res.resolve("amazon.com", "https");

  BOOST_REQUIRE(conn.uses_ssl());
  conn.set_host("gitlab.com");
  boost::system::error_code ec;
  conn.connect(eps.begin()->endpoint(), ec);
  BOOST_CHECK(ec);
}

BOOST_COROUTINE_TEST_CASE(async_echo)
{
  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.async_ropen(http::verb::post, "/echo", hd, ss, nullptr, yield);
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK(s.async_read(dbuf, yield) == 9);
  BOOST_CHECK(str == "test-data");
  BOOST_CHECK(s.headers().at("test") == "dummy");
  conn.async_close(yield);
}

BOOST_COROUTINE_TEST_CASE(async_echo_chunked)
{
  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.async_ropen(http::verb::post, "/echo-chunked", hd, ss, nullptr, yield);
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK(s.async_read(dbuf, yield) == 9);
  BOOST_CHECK(str == "test-data");
  BOOST_CHECK(s.headers().at("test") == "dummy");
  conn.async_close(yield);
}

BOOST_COROUTINE_TEST_CASE(async_boost_get_http)
{
  requests::connection conn{yield.get_executor()};
  asio::ip::tcp::resolver res{yield.get_executor()};
  auto eps = res.async_resolve("amazon.com", "http", yield);
  conn.async_connect(eps.begin()->endpoint(), yield);

  BOOST_REQUIRE(!conn.uses_ssl());
  namespace http = boost::beast::http;

  requests::http::headers hd;
  auto src = requests::make_source(requests::empty{});
  conn.async_ropen(http::verb::get, "/echo", hd, *src, nullptr, yield).async_dump(yield);
  conn.close();
}

BOOST_COROUTINE_TEST_CASE(async_amazon_get_https)
{
  asio::ssl::context sslctx{asio::ssl::context_base::tlsv12_client};
  sslctx.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
  sslctx.set_default_verify_paths();
  requests::connection conn{yield.get_executor(), sslctx};
  asio::ip::tcp::resolver res{yield.get_executor()};
  auto eps = res.async_resolve("amazon.com", "https", yield);
  conn.async_connect(eps.begin()->endpoint(), yield);

  BOOST_REQUIRE(conn.uses_ssl());
  namespace http = boost::beast::http;

  requests::http::headers hd;
  auto src = requests::make_source(requests::empty{});
  conn.async_ropen(http::verb::get, "/echo", hd, *src, nullptr, yield).async_dump(yield);
  conn.close();
}

BOOST_AUTO_TEST_CASE(dump)
{
  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.ropen(http::verb::post, "/echo", hd, ss, nullptr);
  BOOST_CHECK(s.headers().at("test") == "dummy");
  s.dump();

  conn.close();
}

BOOST_COROUTINE_TEST_CASE(async_dump)
{
  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.async_ropen(http::verb::post, "/echo", hd, ss, nullptr, yield);
  s.async_dump(yield);
  BOOST_CHECK(s.headers().at("test") == "dummy");
  conn.async_close(yield);
}


BOOST_AUTO_TEST_CASE(dump_chunked)
{
  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.ropen(http::verb::post, "/echo-chunked", hd, ss, nullptr);
  BOOST_CHECK(s.headers().at("test") == "dummy");
  s.dump();

  conn.close();
}

BOOST_COROUTINE_TEST_CASE(async_dump_chunked)
{
  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = conn.async_ropen(http::verb::post, "/echo-chunked", hd, ss, nullptr, yield);
  s.async_dump(yield);
  BOOST_CHECK(s.headers().at("test") == "dummy");
  conn.async_close(yield);
}

BOOST_AUTO_TEST_CASE(upgrade)
{
  asio::io_context ctx;
  requests::connection conn{ctx};
  conn.connect(endpoint());

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = std::move(conn).upgrade("/ws/echo", hd, nullptr);
  BOOST_REQUIRE(s.is_open());
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK_EQUAL(s.read(dbuf), 12);
  BOOST_CHECK_EQUAL(str, "Hello World!");
  BOOST_CHECK(!conn.is_open());
}



BOOST_COROUTINE_TEST_CASE(async_upgrade)
{
  requests::connection conn{yield.get_executor()};
  conn.async_connect(endpoint(), yield);

  namespace http = boost::beast::http;

  requests::http::headers hd{{"test", "dummy"}};
  requests::string_source ss{"test-data"};
  auto s = std::move(conn).async_upgrade("/ws/echo", hd, nullptr, yield);
  BOOST_REQUIRE(s.is_open());
  std::string str;
  auto dbuf = asio::dynamic_buffer(str);
  BOOST_CHECK_EQUAL(s.async_read(dbuf, yield), 12);
  BOOST_CHECK_EQUAL(str, "Hello World!");
  BOOST_CHECK(!conn.is_open());
}

BOOST_AUTO_TEST_SUITE_END();
