// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/json.hpp>
#include <boost/requests/source.hpp>
#include <boost/requests/request_settings.hpp>

#include "doctest.h"
#include "string_maker.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <thread>
#include <boost/beast/http/read.hpp>

using namespace boost;

TEST_SUITE_BEGIN("source");

TEST_CASE("sync")
{
  asio::io_context ctx;
  asio::readable_pipe rp{ctx};
  asio::writable_pipe wp{ctx};
  asio::connect_pipe(rp, wp);

  std::thread thr{
      [&]{
        system::error_code ec;
        requests::http::request<beast::http::empty_body> req{requests::http::verb::get, "/test", 11};
        auto sp = tag_invoke(requests::make_source_tag{}, json::value{"foobaria"});
        write_request(wp,
                      std::move(req),
                      sp,
                      ec);
        CHECK(ec == system::error_code{});
      }};


  requests::http::request<beast::http::string_body> req;
  beast::flat_buffer buf;
  system::error_code ec;
  beast::http::read(rp, buf, req, ec);
  CHECK(ec == system::error_code{});
  CHECK(req.method() == beast::http::verb::get);
  CHECK(req.target() == "/test");
  CHECK(req.at(boost::beast::http::field::content_type) == "application/json");
  CHECK(json::parse(req.body()) == json::value{"foobaria"});
  thr.join();
}

asio::awaitable<void> async_impl()
{
  asio::readable_pipe rp{co_await asio::this_coro::executor};
  asio::writable_pipe wp{co_await asio::this_coro::executor};
  auto sp = tag_invoke(requests::make_source_tag{}, json::string("foobaria"));
  asio::connect_pipe(rp, wp);
  {
    requests::http::request<beast::http::empty_body> req{requests::http::verb::get, "/test", 11};
    async_write_request(wp,
                        std::move(req),
                        sp,
                        asio::detached);
  }

  requests::http::request<beast::http::string_body> req;
  beast::flat_buffer buf;
  system::error_code ec;
  co_await beast::http::async_read(rp, buf, req, asio::use_awaitable);
  CHECK(req.method() == beast::http::verb::get);
  CHECK(req.target() == "/test");
  CHECK(req.at(boost::beast::http::field::content_type) == "application/json");
  CHECK(json::parse(req.body()) == "foobaria");

  co_return ;
}

TEST_CASE("async")
{
  asio::io_context ctx;
  asio::co_spawn(ctx, async_impl(), [&](std::exception_ptr e){CHECK(e == nullptr);});
  ctx.run();
}


TEST_SUITE_END();