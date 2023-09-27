//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_REQUESTS_YIELD \
  BOOST_ASIO_CORO_YIELD      \
    if (bool _once_ = true)  \
        for (BOOST_ASIO_HANDLER_LOCATION((__FILE__, __LINE__, op_name)); _once_; _once_= false)

#define BOOST_REQUESTS_AWAIT_LOCK(Mutex, Lock) \
  if (!Mutex.try_lock())   \
  {                       \
    BOOST_REQUESTS_YIELD Mutex.async_lock(std::move(self));             \
    if (ec)               \
       break;             \
  }                       \
  Lock = detail::lock_guard{Mutex, std::adopt_lock}

