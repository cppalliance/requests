//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_IMPL_CONDITION_VARIABLE_IPP
#define BOOST_REQUESTS_DETAIL_IMPL_CONDITION_VARIABLE_IPP

#include <boost/requests/detail/condition_variable.hpp>

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

void condition_variable::async_wait(std::unique_lock<std::mutex> & lock,
                                    faux_token_t<void(system::error_code)> tk)
{
  std::weak_ptr<int> indicator = this->shutdown_indicator_;
  lock.unlock();
  timer_.async_wait(
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
#endif // BOOST_REQUESTS_DETAIL_IMPL_CONDITION_VARIABLE_IPP
