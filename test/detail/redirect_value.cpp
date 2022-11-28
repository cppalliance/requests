//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/redirect_value.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include "../doctest.h"


TEST_CASE("redirect_value")
{
  using namespace boost;
  asio::io_context ctx;
  auto tpl = asio::post(ctx, asio::append(asio::deferred, system::error_code{}, 42));

  int i = -1;
  std::future<void> res = tpl(
      requests::detail::redirect_value( asio::use_future, i));

  ctx.run();

  res.get();
  CHECK(i == 42);
}