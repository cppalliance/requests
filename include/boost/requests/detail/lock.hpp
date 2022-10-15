//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_ERR_LOCK_HPP
#define BOOST_REQUESTS_DETAIL_ERR_LOCK_HPP

#include <boost/asem/lock_guard.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/exchange.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/detail/except.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename Mutex>
void try_lock(Mutex & mtx, system::error_code & ec)
{
  if (!mtx.try_lock())
    ec = asio::error::in_progress;
}

template<typename Mutex>
struct lock {
  Mutex * mtx = nullptr;

  lock() = default;

  lock(Mutex & mtx, system::error_code & ec)
  {
    if (mtx.try_lock())
      this->mtx = &mtx;
    else
      ec = asio::error::in_progress;
  }

  lock(Mutex & mtx)
  {
    if (mtx.try_lock())
      this->mtx = &mtx;
    else
      urls::detail::throw_system_error(asio::error::in_progress);
  }

  lock(lock && lhs)
  {
    std::swap(lhs.mtx, mtx);
  }

  lock & operator=(lock && lhs)
  {
    std::swap(lhs.mtx, mtx);
    return *this;
  }

  ~lock()
  {
    if (mtx)
      mtx->unlock();
  }

  constexpr operator bool() const {return mtx != nullptr;}
};

template<typename Mutex1, typename Mutex2,
          BOOST_ASEM_COMPLETION_TOKEN_FOR(void(system::error_code, asem::lock_guard<Mutex1>, asem::lock_guard<Mutex2>)) CompletionToken
              BOOST_ASEM_DEFAULT_COMPLETION_TOKEN_TYPE(typename Mutex1::executor_type) >
inline BOOST_ASEM_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(system::error_code, asem::lock_guard<Mutex>, asem::lock_guard<Mutex>))
async_double_lock(Mutex2 && mtx1, Mutex2 && mtx2, CompletionToken &&token)
{
  using namespace asio::experimental;

  return make_parallel_group(
      asem::async_lock(mtx1, asio::deferred),
      asem::async_lock(mtx2, asio::deferred))
      .async_wait(wait_for_all(),
                  asio::deferred([](std::array<std::size_t, 2u>,
                     system::error_code ec1,
                     asem::lock_guard<Mutex1> lock1,
                     system::error_code ec2,
                     asem::lock_guard<Mutex2> lock2)
                  {
                    return asio::deferred.values(
                                       ec1 ? ec1 : ec2,
                                       std::move(lock1),
                                       std::move(lock2));
                  }))(std::forward<CompletionToken>(token));
}

}
}
}

#endif // BOOST_REQUESTS_DETAIL_ERR_LOCK_HPP
