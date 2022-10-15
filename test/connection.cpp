// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include "doctest.h"
#include "boost/requests/op/auth.hpp"

#include <boost/asio/ssl/host_name_verification.hpp>
#include <iostream>

using namespace boost;

inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}

TEST_SUITE_BEGIN("connection");

TEST_CASE("ssl-detect")
{
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context::tlsv11};


  requests::http_connection conn{ctx};
  requests::https_connection sconn{ctx, sslctx};

  CHECK(requests::detail::get_ssl_layer(conn) == nullptr);
  CHECK(requests::detail::get_ssl_layer(sconn) == &sconn.next_layer());
}

TEST_CASE("sync-http-request")
{
  auto url = httpbin();

  asio::io_context ctx;

  requests::http_connection hc{ctx};

  hc.set_host(url);

  asio::ip::tcp::resolver rslvr{ctx};
  auto ep = *rslvr.resolve(url, "http").begin();

  hc.connect(ep);

  beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
  beast::http::response<beast::http::string_body> res;

  hc.single_request(req, res);

  std::cout << "<" << res << ">" << std::endl;
  auto js = json::parse(res.body());

  CHECK(js.at("headers").at("Host").as_string() == url);
  CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);
}


TEST_CASE("sync-https-request")
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};
  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();
  sslctx.set_verify_callback(asio::ssl::host_name_verification(url));

  requests::https_connection hc{ctx, sslctx};

  hc.set_host(url);


  asio::ip::tcp::resolver rslvr{ctx};
  auto ep = *rslvr.resolve(url, "https").begin();

  hc.connect(ep);

  beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
  beast::http::response<beast::http::string_body> res;

  hc.single_request(req, res);

  std::cout << "<" << res << ">" << std::endl;
  auto js = json::parse(res.body());

  CHECK(js.at("headers").at("Host").as_string() == url);
  CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);
}



asio::awaitable<void> async_http_exception()
{
  auto url = httpbin();

  using exec_t = typename asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
  exec_t exec{co_await asio::this_coro::executor};

  requests::basic_http_connection<exec_t> hc{exec};;
  hc.set_host(url);

  typename asio::ip::tcp::resolver::rebind_executor<exec_t>::other rslvr{exec};
  auto ep = *(co_await rslvr.async_resolve(url, "http")).begin();

  co_await hc.async_connect(ep);

  beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
  beast::http::response<beast::http::string_body> res;

  co_await hc.async_single_request(req, res);

  std::cout << "<" << res << ">" << std::endl;
  auto js = json::parse(res.body());

  CHECK(js.at("headers").at("Host").as_string() == url);
  CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);
}


TEST_CASE("async-http-exception")
{
  asio::io_context ctx;
  bool called = false;

  asio::co_spawn(ctx,
                 async_http_exception,
                 [&](std::exception_ptr e)
                 {
                   CHECK(e == nullptr);
                   called = true;
                 });

  ctx.run();
  CHECK(called);
}


asio::awaitable<void> async_https_exception()
{
  auto url = httpbin();

  using exec_t = typename asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
  exec_t exec{co_await asio::this_coro::executor};

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};
  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();
  sslctx.set_verify_callback(asio::ssl::host_name_verification(url));

  requests::basic_https_connection<exec_t> hc{exec, sslctx};;

  hc.set_host(url);

  typename asio::ip::tcp::resolver::rebind_executor<exec_t>::other rslvr{exec};
  auto ep = *(co_await rslvr.async_resolve(url, "https")).begin();

  co_await hc.async_connect(ep);
  beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
  beast::http::response<beast::http::string_body> res;

  co_await hc.async_single_request(req, res);

  std::cout << "<" << res << ">" << std::endl;
  auto js = json::parse(res.body());

  CHECK(js.at("headers").at("Host").as_string() == url);
  CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);
}

TEST_CASE("async-https-exception")
{
  asio::io_context ctx;
  bool called = false;
  asio::co_spawn(ctx,
                 async_https_exception,
                 [&](std::exception_ptr e)
                 {
                   called = true;
                   CHECK(e == nullptr);
                 });

  ctx.run();
  CHECK(called);
}



TEST_CASE("sync-rget-exception")
{
    std::string url = "https://httpbin.org/";
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
        url = p;

    asio::io_context ctx;
    asio::ssl::context sslctx{asio::ssl::context::tls_client};

    requests::http_connection conn{ctx};
    requests::https_connection sconn{ctx, sslctx};

    CHECK(requests::detail::get_ssl_layer(conn) == nullptr);
    CHECK(requests::detail::get_ssl_layer(sconn) == &sconn.next_layer());



    /*conn.connect_to_host(urls::parse_uri(url)->encoded_host());
    auto res = conn.request(
            beast::http::verb::get,
            "/get?foo=bar",
            {},
            requests::empty{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    auto js = json::parse(res.body());
    CHECK(js.at("args").at("foo") == "bar");

    res = conn.request(
            beast::http::verb::get,
            "/redirect-to?url=%2Fget%3Fredirect%3Dworked&status_code=301",
            {.follow_redirects=true},
            requests::empty{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("args").at("redirect") == "worked");


    res = conn.request(
            beast::http::verb::get,
            "/basic-auth/test-user/test-password",
            {.follow_redirects=true},
            requests::empty{},
            requests::basic_auth{"test-user", "test-password"});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("authenticated") == true);
    CHECK(js.at("user") == "test-user");
*/
}

/*
TEST_CASE("sync-rget-error_code")
{
    std::string url = "https://httpbin.org/";
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
        url = p;

    asio::io_context ctx;
    asio::ssl::context sslctx{asio::ssl::context::tls_client};

    requests::https_connection conn{ctx, sslctx};

    system::error_code ec;

    conn.connect_to_host(urls::parse_uri(url)->encoded_host(), ec);
    REQUIRE(ec == system::error_code{});
    auto res = conn.request(
            beast::http::verb::get,
            "/get?bar=foo",
            {},
            requests::empty{}, ec);
    REQUIRE(ec == system::error_code{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    auto js = json::parse(res.body());
    CHECK(js.at("args").at("bar") == "foo");



    res = conn.get(
            "/redirect-to?url=%2Fget%3Fredirect%3Dworked&status_code=301",
            {.follow_redirects=true}, ec);
    REQUIRE(ec == system::error_code{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("args").at("redirect") == "worked");

    res = conn.request(
            beast::http::verb::get,
            "/bearer",
            {.follow_redirects=true},
            requests::empty{},
            requests::bearer{"test-token"});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("authenticated") == true);
    CHECK(js.at("token") == "test-token");
}


asio::awaitable<void> async_rget()
{
    std::string url = "https://httpbin.org/";
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
        url = p;

    using def_exec = asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
    using conn_type = requests::basic_connection<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<def_exec>::other>>;

    asio::ssl::context sslctx{asio::ssl::context::tls_client};
    conn_type conn{co_await asio::this_coro::executor, sslctx};

    co_await conn.async_connect_to_host(urls::parse_uri(url)->encoded_host());
    auto res = co_await conn.async_request(
            beast::http::verb::get,
            "/get?foo=bar",
            {},
            requests::empty{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    auto js = json::parse(res.body());
    CHECK(js.at("args").at("foo") == "bar");

    res = co_await conn.async_request(
            beast::http::verb::get,
            "/redirect-to?url=%2Fget%3Fredirect%3Dworked&status_code=301",
            {.follow_redirects=true},
            requests::empty{});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("args").at("redirect") == "worked");

    res = co_await conn.async_request(
            beast::http::verb::get,
            "/basic-auth/test-user/test-password",
            {.follow_redirects=true},
            requests::empty{},
            requests::basic_auth{"test-user", "test-password"});

    MESSAGE(res.body());
    CHECK(res.result() == beast::http::status::ok);
    js = json::parse(res.body());
    CHECK(js.at("authenticated") == true);
    CHECK(js.at("user") == "test-user");
}

TEST_CASE("async-rget")
{
    asio::io_context ctx;
    auto ft = asio::co_spawn(ctx, async_rget(), asio::use_future);

    ctx.run();
    CHECK_NOTHROW(ft.get());
}

TEST_CASE("sync-ws-exception")
{
    std::string url = "ws://ws.ifelse.io/";
    if (auto p = ::getenv("BOOST_REQUEST_ECHO_SERVER"))
        url = p;

    asio::io_context ctx;
    requests::http_connection conn{ctx};

    conn.connect_to_host(urls::parse_uri(url)->encoded_host());
    auto res = std::move(conn).handshake("/", {});
    res.write(asio::buffer("test-string"));

    beast::flat_buffer fb;
    res.read(fb);
}


TEST_CASE("sync-ws-error_code")
{
    std::string url = "ws://ws.ifelse.io/";
    if (auto p = ::getenv("BOOST_REQUEST_ECHO_SERVER"))
        url = p;

    asio::io_context ctx;
    requests::http_connection conn{ctx};

    system::error_code ec;

    conn.connect_to_host(urls::parse_uri(url)->encoded_host(), ec);
    REQUIRE(ec == system::error_code{});
    auto res = std::move(conn).handshake("/", {}, ec);
    REQUIRE(ec == system::error_code{});
    res.write(asio::buffer("test-string"));

    beast::flat_buffer fb;
    res.read(fb);
}

asio::awaitable<void> async_ws()
{
    std::string url = "ws://ws.ifelse.io/";
    if (auto p = ::getenv("BOOST_REQUEST_ECHO_SERVER"))
        url = p;


    auto conn = asio::use_awaitable.as_default_on(requests::http_connection{co_await asio::this_coro::executor});

    co_await conn.async_connect_to_host(urls::parse_uri(url)->encoded_host());
    auto res = co_await std::move(conn).async_handshake(url, {});
    co_await res.async_write(asio::buffer("test-string"));

    beast::flat_buffer fb;
    co_await res.async_read(fb);
}

TEST_CASE("async-ws")
{
    asio::io_context ctx;
    auto ft = asio::co_spawn(ctx, async_ws(), asio::use_future);

    ctx.run();
    CHECK_NOTHROW(ft.get());
}

*/
TEST_SUITE_END();