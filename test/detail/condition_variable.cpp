//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/condition_variable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <thread>

#include "../string_maker.hpp"


TEST_SUITE_BEGIN("condition_variable");

namespace asio      = boost::asio;
namespace requests  = boost::requests;

TEST_CASE("sync")
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
      CHECK_THROWS(cv.wait(lock));
      pos = 3;
    }};

    CHECK(pos == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cv.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(pos == 1);
    cv.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(pos == 2);

  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  CHECK(pos == 3);


  thr.join();
  ctx.join();
}

TEST_SUITE_END();