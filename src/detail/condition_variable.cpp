//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/condition_variable.hpp>
#include <boost/asio/deferred.hpp>


namespace boost
{
namespace requests
{
namespace detail
{

void condition_variable::wait(std::unique_lock<std::mutex> & lock, system::error_code & ec)
{
  std::weak_ptr<int> indicator = this->shutdown_indicator_;
  cv_.wait(lock);
  if (indicator.expired())
    ec = asio::error::operation_aborted;
}

void condition_variable::async_wait_impl_(
    asio::any_completion_handler<void(system::error_code)> tk,
    condition_variable * this_,
    std::unique_lock<std::mutex> & lock)
{
  std::weak_ptr<int> indicator = this_->shutdown_indicator_;
  lock.unlock();
  this_->timer_.async_wait(
        asio::deferred(
          [&lock, indicator](system::error_code ec_)
          {
            lock.lock();
            if (!indicator.expired())
              ec_.clear();
            return asio::deferred.values(ec_);
          }))
          (std::move(tk));
}

condition_variable::~condition_variable()
{
  shutdown_indicator_.reset();
  timer_.cancel();
  cv_.notify_all();
}

}
}
}
