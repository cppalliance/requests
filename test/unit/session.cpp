// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/session.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/json.hpp>
#include <boost/test/unit_test.hpp>

#include <thread>

#include "../coroutine_test_case.hpp"
#include "../fixtures/server.hpp"

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
using boost::system::error_code ;

BOOST_FIXTURE_TEST_SUITE(session, test_server);

BOOST_AUTO_TEST_CASE(sync)
{
  asio::io_context ctx;
  requests::session sess{ctx};
  auto & pool = *sess.get_pool("unix://" + std::string(path()));
  BOOST_CHECK(pool.active() == 0u);
  BOOST_CHECK(pool.free() == 0u);
  BOOST_CHECK(pool.endpoints().size() == 1u);

  auto & p2 = *sess.get_pool("unix://" + std::string(path()));
  BOOST_CHECK(&pool == &p2);
}

BOOST_COROUTINE_TEST_CASE(async)
{
  requests::session sess{yield.get_executor()};
  auto & pool = *sess.async_get_pool("unix://" + std::string(path()), yield);
  BOOST_CHECK(pool.active() == 0u);
  BOOST_CHECK(pool.free() == 0u);
  BOOST_CHECK(pool.endpoints().size() == 1u);

  auto & p2 = *sess.async_get_pool("unix://" + std::string(path()), yield);
  BOOST_CHECK(&pool == &p2);
}


BOOST_AUTO_TEST_SUITE_END();