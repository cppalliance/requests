//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_TRACKER_HPP
#define BOOST_REQUESTS_TRACKER_HPP

#include <atomic>
#include <boost/core/exchange.hpp>

namespace boost {
namespace requests {
namespace detail {

struct tracker
{
  std::atomic<std::size_t> *cnt = nullptr;
  tracker() = default;
  tracker(std::atomic<std::size_t> &cnt) : cnt(&cnt) {++cnt;}
  ~tracker()
  {
    if (cnt) --(*cnt);
  }

  tracker(const tracker &) = delete;
  tracker(tracker && lhs) noexcept : cnt(boost::exchange(lhs.cnt, nullptr))
  {
  }

  tracker& operator=(tracker && lhs) noexcept
  {
    std::swap(cnt, lhs.cnt);
    return *this;
  }
};

}
}
}

#endif // BOOST_REQUESTS_TRACKER_HPP
