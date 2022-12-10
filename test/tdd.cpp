// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection_pool.hpp>
#include <boost/requests/method.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <boost/json.hpp>
#include "doctest.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast.hpp>

using namespace boost;


TEST_CASE("executor-trait")
{
  asio::io_context ctx;
  requests::http_connection conn{ctx};

  requests::http::request<requests::http::empty_body> req{
      requests::http::verb::connect,
      "www.google.com:80",
      11
  };
  //req.set(requests::http::field::)

  system::error_code ec;
  conn.set_host("127.0.0.1");
  conn.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 8888));


  auto r = conn.ropen(req, {}, nullptr, ec);
  std::cout << "Ec: " << ec.message() << std::endl;
  std::cout << "H: " << r.headers() << std::endl;

  std::string res;
  auto buf = asio::dynamic_buffer(res);
  r.read(buf);
  conn.set_host("www.google.com");


  std::cout << "Resp: " << res << std::endl;
  auto rr = get(conn, urls::url_view("/index.html"), {.opts={.enforce_tls=false}});

  std::cout << "R : " << rr.headers << std::endl;

}
