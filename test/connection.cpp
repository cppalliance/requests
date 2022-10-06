// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
#include <boost/json.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include "doctest.h"
#include "boost/requests/op/auth.hpp"

using namespace boost;

TEST_SUITE_BEGIN("connection");

TEST_CASE("sync-rget-exception")
{
    std::string url = "https://httpbin.org/";
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
        url = p;

    asio::io_context ctx;
    asio::ssl::context sslctx{asio::ssl::context::tls_client};

    requests::https_connection conn{ctx, sslctx};

    conn.connect_to_host(urls::parse_uri(url)->encoded_host());
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
}


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


TEST_SUITE_END();