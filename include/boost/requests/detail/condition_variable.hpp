//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP
#define BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP

#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/requests/detail/config.hpp>
#include <condition_variable>

namespace boost {
namespace requests {
namespace detail {

struct condition_variable
{
  using executor_type = asio::any_io_executor;

  explicit condition_variable(executor_type exec)
      : timer_(exec, std::chrono::steady_clock::time_point::max()) {}

  template<typename ExecutionContext>
  explicit condition_variable(ExecutionContext & ctx,
                 typename std::enable_if<
                     std::is_convertible<
                         ExecutionContext&,
                         asio::execution_context&>::value
                     >::type * = nullptr)
      : timer_(ctx, std::chrono::steady_clock::time_point::max())
  {
  }

  BOOST_REQUESTS_DECL void
  async_wait(std::unique_lock<std::mutex> & lock, faux_token_t<void(system::error_code)> tk);

  condition_variable& operator=(const condition_variable&) = delete;
  condition_variable& operator=(condition_variable&& lhs) noexcept = default;
  condition_variable(const condition_variable&) = delete;
  condition_variable(condition_variable&& mi) noexcept = default;
  BOOST_REQUESTS_DECL void wait(std::unique_lock<std::mutex> & lock, system::error_code & ec);
  void wait(std::unique_lock<std::mutex> & lock)
  {
    system::error_code ec;
    wait(lock, ec);
    if (ec)
      boost::throw_exception(system::system_error(ec, "lock"));
  }
  void notify_one() { if (timer_.cancel_one() == 0u) cv_.notify_one(); }
  void notify_all() { timer_.cancel(); cv_.notify_all();}

  template <typename Executor1>
  struct rebind_executor
  {
    /// The mutex type when rebound to the specified executor.
    typedef condition_variable other;
  };

  executor_type
  get_executor() noexcept {return timer_.get_executor();}

  BOOST_REQUESTS_DECL ~condition_variable();

 private:
  asio::steady_timer timer_;
  std::condition_variable cv_;
  std::shared_ptr<int> shutdown_indicator_{std::make_shared<int>()};
};

}
}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/detail/impl/condition_variable.ipp>
#endif


#endif // BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP
