//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/condition_variable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <thread>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(condition_variable);

namespace asio      = boost::asio;
namespace requests  = boost::requests;

BOOST_AUTO_TEST_CASE(sync)
{
  asio::thread_pool ctx;
  std::thread thr;

  std::mutex mtx;
  std::unique_lock<std::mutex> lock{mtx};

  int pos = 0;
  {
    requests::detail::condition_variable cv{ctx};

    thr = std::thread{[&] {
      cv.wait(lock);
      pos = 1;
      cv.wait(lock);
      pos = 2;
      BOOST_CHECK_THROW(cv.wait(lock), boost::system::system_error);
      pos = 3;
    }};

    BOOST_CHECK(pos == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cv.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    BOOST_CHECK(pos == 1);
    cv.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    BOOST_CHECK(pos == 2);

  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_CHECK(pos == 3);


  thr.join();
  ctx.join();
}

BOOST_AUTO_TEST_SUITE_END();