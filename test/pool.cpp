// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection_pool.hpp>
#include <boost/requests/op.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/json.hpp>

#include <boost/url/url.hpp>
#include "doctest.h"
#include <iostream>

using namespace boost;
using asio::experimental::awaitable_operators::operator&&;

TEST_SUITE_BEGIN("pool");
/*
asio::awaitable<void> async_test()
{
    std::string url = "https://httpbin.org/";
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
        url = p;

    using def_exec = asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
    using pool_type = requests::basic_connection_pool<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<def_exec>::other>>;

    asio::ssl::context sslctx{asio::ssl::context::tls_client};
    pool_type conn{co_await asio::this_coro::executor, sslctx, "httpbin.org", 10};

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




TEST_CASE("async-pool-request")
{
    asio::io_context ctx;
    auto ft = asio::co_spawn(ctx, async_test(), asio::use_future);

    ctx.run();
    CHECK_NOTHROW(ft.get());
}



asio::awaitable<void> async_pool_ws()
{
    std::string url = "ws://ws.ifelse.io/";
    if (auto p = ::getenv("BOOST_REQUEST_ECHO_SERVER"))
        url = p;


    auto pool = asio::use_awaitable.as_default_on(
            requests::http_connection_pool{co_await asio::this_coro::executor, "ifelse.io"});



    auto ws1 = co_await pool.async_handshake(url, {});
    auto ws2 = co_await  pool.async_handshake(url, {});

    co_await (ws1.async_write(asio::buffer("test-string")) && ws2.async_write(asio::buffer("test-string")));

    beast::flat_buffer fb, f2;
    co_await (ws1.async_read(fb) && ws2.async_read(f2));
}

TEST_CASE("async-ws")
{
    asio::io_context ctx;
    auto ft = asio::co_spawn(ctx, async_pool_ws(), asio::use_future);

    ctx.run();
    CHECK_NOTHROW(ft.get());
}
*/

TEST_SUITE_END();