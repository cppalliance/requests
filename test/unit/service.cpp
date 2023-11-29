//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/service.hpp>
#include <boost/url/parse.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(service);

BOOST_AUTO_TEST_CASE(session)
{
  using namespace boost;

  asio::io_context ctx;
  asio::thread_pool tp;

  asio::any_io_executor exec{ctx.get_executor()};

  auto & s1 = requests::default_session(tp.get_executor());
  auto & s2 = requests::default_session(ctx.get_executor());
  auto & s3 = requests::default_session(exec);

  BOOST_CHECK(&s1 != & s2);
  BOOST_CHECK(&s2 == & s3);
}

BOOST_AUTO_TEST_SUITE_END();
