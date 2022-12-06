//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/method.hpp>
#include <boost/requests/service.hpp>
#include <boost/url/parse.hpp>
#include <boost/asio/thread_pool.hpp>
#include <vector>
#include "doctest.h"
#include "string_maker.hpp"


TEST_SUITE_BEGIN("service");

TEST_CASE("session")
{
  using namespace boost;

  asio::io_context ctx;
  asio::thread_pool tp;

  asio::any_io_executor exec{ctx.get_executor()};

  get(requests::default_session(exec), urls::parse_uri("https://httpbin.org").value(), {});
  requests::default_session(tp.get_executor());
  requests::default_session(ctx.get_executor() );
}

TEST_SUITE_END();