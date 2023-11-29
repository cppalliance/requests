//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "httpbin.hpp"

boost::requests::connection httpbin::connect()
{
  using namespace boost;
  const bool https = url().scheme_id() == urls::scheme::https;
  auto hc = https ? requests::connection(ctx.get_executor(), sslctx)
                  : requests::connection(ctx.get_executor());
  hc.set_host(url().encoded_host_and_port());
  hc.use_ssl(https);

  asio::ip::tcp::resolver rslvr{ctx};
  core::string_view service = https ? "https" : "http";
  if (url().has_port())
    service = url().port();
  asio::ip::tcp::endpoint ep = *rslvr.resolve(url().encoded_host(), service).begin();

  hc.connect(ep);

  return hc;
}

boost::requests::connection httpbin::async_connect(boost::asio::yield_context yield)
{
  using namespace boost;

  const bool https = url().scheme_id() == urls::scheme::https;
  auto hc = https ? requests::connection(yield.get_executor(), sslctx)
                  : requests::connection(yield.get_executor());
  hc.set_host(url().encoded_host_and_port());
  hc.use_ssl(https);

  asio::ip::tcp::resolver rslvr{yield.get_executor()};
  core::string_view service = https ? "https" : "http";
  if (url().has_port())
    service = url().port();
  asio::ip::tcp::endpoint ep = *rslvr.async_resolve(url().encoded_host(), service, yield).begin();

  hc.async_connect(ep, yield);

  return hc;
}