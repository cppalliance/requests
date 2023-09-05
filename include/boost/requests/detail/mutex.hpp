//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_MUTEX_HPP
#define BOOST_REQUESTS_DETAIL_MUTEX_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/faux_coroutine.hpp>
#include <list>

namespace boost
{
namespace requests
{
namespace detail
{

struct mutex
{
  using executor_type = asio::any_io_executor;

  explicit mutex(executor_type exec) : exec_(exec) {}

  template<typename ExecutionContext>
  explicit mutex(ExecutionContext & ctx,
                 typename std::enable_if<
                     std::is_convertible<
                         ExecutionContext&,
                         asio::execution_context&>::value
                     >::type * = nullptr)
      : exec_(ctx.get_executor())
  {
  }

  BOOST_REQUESTS_DECL void
  async_lock(faux_token_t<void(system::error_code)> tk);

  mutex& operator=(const mutex&) = delete;
  mutex& operator=(mutex&& lhs) noexcept
  {
    std::lock_guard<std::mutex> _(mtx_);
    exec_ = std::move(lhs.exec_);
    locked_ = locked_.exchange(lhs.locked_.load());
    waiters_ = std::move(lhs.waiters_);
    return *this;
  }

  mutex(const mutex&) = delete;
  mutex(mutex&& mi) noexcept
      : exec_(std::move(mi.exec_)),
        locked_(mi.locked_.exchange(false)),
        waiters_(std::move(mi.waiters_))
  {
  }
  BOOST_REQUESTS_DECL void lock(system::error_code & ec);
  void lock()
  {
    system::error_code ec;
    lock(ec);
    if (ec)
      boost::throw_exception(system::system_error(ec, "lock"));
  }
  BOOST_REQUESTS_DECL void unlock();

  BOOST_REQUESTS_DECL bool try_lock();

  template <typename Executor1>
  struct rebind_executor
  {
    /// The mutex type when rebound to the specified executor.
    typedef mutex other;
  };

  executor_type
  get_executor() const noexcept {return exec_;}

  BOOST_REQUESTS_DECL ~mutex();
 private:

  asio::any_io_executor exec_;
  std::atomic<bool> locked_{false};
  std::mutex mtx_;
  std::list<faux_token_t<void(system::error_code)>> waiters_;
};

}
}
}


#endif // BOOST_REQUESTS_MUTEX_HPP
