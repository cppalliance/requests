//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP
#define BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/compose.hpp>
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

  template<typename CompletionToken>
  auto
  async_wait(std::unique_lock<std::mutex> & lock, CompletionToken && tk)
  {
    return asio::async_compose<CompletionToken, void(system::error_code)>(async_wait_op{this, lock}, tk, timer_);
  }

  condition_variable& operator=(const condition_variable&) = delete;
  BOOST_REQUESTS_DECL
  condition_variable& operator=(condition_variable&& lhs) noexcept;
  condition_variable(const condition_variable&) = delete;
  BOOST_REQUESTS_DECL
  condition_variable(condition_variable&& mi) noexcept;
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

  struct async_wait_op
  {
    condition_variable * this_;
    std::unique_lock<std::mutex> & lock;
    std::weak_ptr<int> indicator;

    template<typename Self>
    void operator()(Self && self)
    {
      indicator = this_->shutdown_indicator_;
      this_->timer_.async_wait(std::move(self));
      lock.unlock();
    }

    template<typename Self>
    void operator()(Self && self, system::error_code ec)
    {
      lock.lock();
      if (!indicator.expired() && self.get_cancellation_state().cancelled() == asio::cancellation_type::none)
        ec.clear();
      self.complete(ec);
    }
  };
};

}
}
}

#endif // BOOST_REQUESTS_DETAIL_CONDITION_VARIABLE_HPP
