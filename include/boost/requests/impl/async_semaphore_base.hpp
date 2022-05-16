//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef BOOST_REQUESTS_IMPL_ASYNC_SEMAPHORE_BASE_HPP
#define BOOST_REQUESTS_IMPL_ASYNC_SEMAPHORE_BASE_HPP

#include <boost/asio/detail/assert.hpp>
#include <boost/asio/error.hpp>
#include <boost/requests/async_semaphore.hpp>
#include <boost/requests/detail/semaphore_wait_op.hpp>

namespace boost::requests
{
async_semaphore_base::async_semaphore_base(int initial_count)
: waiters_()
, count_(initial_count)
{
}

async_semaphore_base::~async_semaphore_base()
{
    auto & nx = waiters_.next_;
    while (nx != &waiters_)
         static_cast< detail::semaphore_wait_op * >(nx)->complete(asio::error::operation_aborted);
}

void
async_semaphore_base::add_waiter(detail::semaphore_wait_op *waiter)
{
    waiter->link_before(&waiters_);
}

int
async_semaphore_base::count() const noexcept
{
    return count_;
}

void
async_semaphore_base::release()
{
    count_ += 1;

    // release a pending operations
    if (waiters_.next_ == &waiters_)
        return;

    decrement();
    static_cast< detail::semaphore_wait_op * >(waiters_.next_)
        ->complete(std::error_code());
}

std::size_t async_semaphore_base::release_all()
{
    std::size_t sz = 0u;
    auto & nx = waiters_.next_;
    while (nx != &waiters_)
    {
        static_cast< detail::semaphore_wait_op * >(nx)->complete(system::error_code());
        sz ++ ;
    }
    return sz;
}


BOOST_ASIO_NODISCARD inline int
async_semaphore_base::value() const noexcept
{
    if (waiters_.next_ == &waiters_)
        return count();

    return count() - static_cast<int>(waiters_.size());
}

bool
async_semaphore_base::try_acquire()
{
    bool acquired = false;
    if (count_ > 0)
    {
        --count_;
        acquired = true;
    }
    return acquired;
}

int
async_semaphore_base::decrement()
{
    BOOST_ASSERT(count_ > 0);
    return --count_;
}

}   // namespace boost::requests

#endif
