//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/mutex.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/core/ignore_unused.hpp>
#include <condition_variable>

namespace boost
{
namespace requests
{
namespace detail
{

void mutex::async_lock_impl_(asio::any_completion_handler<void(system::error_code)> handler,
                             mutex * this_)
{
  std::lock_guard<std::mutex> lock{this_->waiters_mtx_};
  if (this_->try_lock())
  {
    auto exec = asio::get_associated_immediate_executor(handler, this_->exec_);
    return asio::dispatch(exec, asio::append(std::move(handler), system::error_code()));
  }

  auto itr = this_->waiters_.insert(this_->waiters_.end(), std::move(handler));

  auto slot = itr->get_cancellation_slot();
  if (slot.is_connected())
  {
    slot.assign(
        [this_, itr](asio::cancellation_type type)
        {
          if (type != asio::cancellation_type::none)
          {
            std::lock_guard<std::mutex> lock{this_->waiters_mtx_};
            ignore_unused(lock);
            system::error_code ec;
            BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::operation_aborted);
            asio::defer(this_->exec_, asio::append(std::move(*itr), ec));
            this_->waiters_.erase(itr);
          }
        });
  }
}

void mutex::lock(system::error_code & ec)
try {
  this->mutex_.lock();
}
catch (std::system_error & se)
{
  ec = se.code();
}

void mutex::unlock()
{
  std::lock_guard<std::mutex> lock{waiters_mtx_};
  if (waiters_.empty())
    this->mutex_.unlock();
  else if (!waiters_.empty())
  {
    auto h = std::move(waiters_.front());
    h.get_cancellation_slot().clear();
    waiters_.pop_front();
    asio::post(exec_, asio::append(std::move(h), system::error_code{}));
  }
}

bool mutex::try_lock()
{
  return this->mutex_.try_lock();
}

mutex::~mutex()
{
  if (waiters_.empty())
    return ;
  system::error_code ec;
  BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::operation_aborted);

  auto ww = std::move(waiters_);
  for (auto & h : ww)
  {
    h.get_cancellation_slot().clear();
    asio::dispatch(exec_, asio::append(std::move(h), ec));
  }
}

}
}
}
