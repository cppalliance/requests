//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_LOCK_GUARD_HPP
#define BOOST_REQUESTS_DETAIL_LOCK_GUARD_HPP

#include <boost/requests/detail/mutex.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

struct lock_guard
{
  lock_guard() = default;
  lock_guard(const lock_guard &) = delete;
  lock_guard(lock_guard &&lhs) : mtx_(lhs.mtx_)
  {
    lhs.mtx_ = nullptr;
  }

  lock_guard &
  operator=(const lock_guard &) = delete;

  lock_guard &
  operator=(lock_guard &&lhs)
  {
    std::swap(lhs.mtx_, mtx_);
    return *this;
  }

  ~lock_guard()
  {
    if (mtx_ != nullptr)
      mtx_->unlock();
  }
  lock_guard(mutex & mtx, const std::adopt_lock_t &) : mtx_(&mtx) {}
  lock_guard(mutex & mtx) : mtx_(&mtx) {mtx.lock();}
private:

  mutex * mtx_ = nullptr;
};

inline lock_guard lock(mutex & mtx, system::error_code & ec)
{
  mtx.lock(ec);
  if (ec)
    return lock_guard();
  else
    return lock_guard(mtx, std::adopt_lock);
}


}
}
}

#endif // BOOST_REQUESTS_DETAIL_LOCK_GUARD_HPP
