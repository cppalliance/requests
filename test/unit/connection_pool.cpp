// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection_pool.hpp>
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

BOOST_FIXTURE_TEST_SUITE(connection_pool, test_server);

BOOST_AUTO_TEST_CASE(sync)
{
  asio::io_context ctx;
  requests::connection_pool pool{ctx, 3u};
  pool.lookup("unix://" + std::string(path()));

  BOOST_CHECK(pool.active() == 0u);
  auto c1 = pool.borrow_connection();
  BOOST_CHECK(c1.is_open());
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 0u);

  pool.return_connection(std::move(c1));
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 1u);
  BOOST_CHECK(!c1.is_open());

  auto c2 = pool.steal_connection();
  BOOST_CHECK(c2.is_open());
  BOOST_CHECK(pool.active() == 0u);
  BOOST_CHECK(pool.free() == 0u);
  c2.close();
  BOOST_CHECK(!c2.is_open());

  c1 = pool.borrow_connection();
  BOOST_CHECK(c1.is_open());
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 0u);

  c2 = pool.borrow_connection();
  BOOST_CHECK(c2.is_open());
  BOOST_CHECK(pool.active() == 2u);
  BOOST_CHECK(pool.free() == 0u);

  auto c3 = pool.borrow_connection();
  BOOST_CHECK(c3.is_open());
  BOOST_CHECK(pool.active() == 3u);
  BOOST_CHECK(pool.free() == 0u);

  auto c3p = &c3.next_layer();
  decltype(c3p) c4p = nullptr;
  std::atomic_bool bl{false};
  std::thread thr {
      [&]
      {
        auto c4 = pool.borrow_connection();
        c4p = &c4.next_layer();
        pool.remove_connection(std::move(c4));
        bl = true;
      }
  };

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  BOOST_CHECK(!bl);
  pool.return_connection(std::move(c3));
  BOOST_CHECK_EQUAL(pool.active(), 3u);
  // BOOST_CHECK_EQUAL(pool.free(), 1u);// race condition
  BOOST_CHECK(!c3.is_open());


  while (!bl)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  BOOST_CHECK_EQUAL(pool.active(), 2u);
  BOOST_CHECK_EQUAL(pool.free(), 0u);


  BOOST_CHECK(c3p == c4p);
  thr.join();

  BOOST_CHECK(!c3.is_open());
  BOOST_CHECK_EQUAL(pool.active(), 2u);
  BOOST_CHECK_EQUAL(pool.free(), 0u);

  pool.return_connection(std::move(c2));

  BOOST_CHECK(pool.active() == 2u);
  BOOST_CHECK(pool.free() == 1u);
}

BOOST_COROUTINE_TEST_CASE(async)
{
  requests::connection_pool pool{yield.get_executor(), 2u};
  pool.async_lookup("unix://" + std::string(path()), yield);

  BOOST_CHECK(pool.active() == 0u);
  auto c1 = pool.async_borrow_connection(yield);
  BOOST_CHECK(c1.is_open());
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 0u);

  pool.return_connection(std::move(c1));
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 1u);
  BOOST_CHECK(!c1.is_open());

  auto c2 = pool.async_steal_connection(yield);
  BOOST_CHECK(c2.is_open());
  BOOST_CHECK(pool.active() == 0u);
  BOOST_CHECK(pool.free() == 0u);
  c2.close();
  BOOST_CHECK(!c2.is_open());
  c1 = pool.async_borrow_connection(yield);
  BOOST_CHECK(c1.is_open());
  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 0u);

  c2 = pool.async_borrow_connection(yield);
  BOOST_CHECK(c2.is_open());
  BOOST_CHECK(pool.active() == 2u);
  BOOST_CHECK(pool.free() == 0u);

  auto c2p = &c2.next_layer();
  decltype(c2p) c3p = nullptr;


  bool bl = false;
  pool.async_borrow_connection(
      [&](boost::system::error_code ec, requests::connection conn)
      {
        c3p = &conn.next_layer();
        pool.remove_connection(std::move(conn));
        bl = true;
      });


  BOOST_CHECK(!bl);
  pool.return_connection(std::move(c2));
  BOOST_CHECK_EQUAL(pool.active(), 2u);
  // BOOST_CHECK_EQUAL(pool.free(), 1u);// race condition
  BOOST_CHECK(!c2.is_open());


  while (!bl)
    asio::post(yield);

  BOOST_CHECK_EQUAL(pool.active(), 1u);
  BOOST_CHECK_EQUAL(pool.free(), 0u);


  BOOST_CHECK(c3p == c2p);

  BOOST_CHECK(!c2.is_open());
  BOOST_CHECK_EQUAL(pool.active(), 1u);
  BOOST_CHECK_EQUAL(pool.free(), 0u);

  pool.return_connection(std::move(c1));

  BOOST_CHECK(pool.active() == 1u);
  BOOST_CHECK(pool.free() == 1u);
}


BOOST_AUTO_TEST_SUITE_END();