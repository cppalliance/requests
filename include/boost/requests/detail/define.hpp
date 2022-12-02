//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define await(coro) \
  coro = {}; \
  while (!coro.is_complete()) \
    yield

#define await_lock(Mutex, Lock) \
  if (!Mutex.try_lock())   \
  {                       \
    yield Mutex.async_lock(std::move(self)); \
    if (ec)               \
       break;             \
  }                       \
  Lock = asem::lock_guard<decltype(Mutex)>{Mutex, std::adopt_lock}
