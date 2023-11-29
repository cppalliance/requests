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
condition_variable::~condition_variable()
{
  shutdown_indicator_.reset();
  timer_.cancel();
  cv_.notify_all();
}

condition_variable& condition_variable::operator=(condition_variable&& lhs) noexcept
{
  shutdown_indicator_ = std::make_shared<int>();
  timer_.cancel();
  cv_.notify_all();
  timer_ = asio::steady_timer(lhs.timer_.get_executor());
  return *this;
}


condition_variable::condition_variable(condition_variable&& mi) noexcept
  : timer_(mi.timer_.get_executor())
{
}

}
}
}
