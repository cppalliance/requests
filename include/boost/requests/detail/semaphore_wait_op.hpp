//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef BOOST_REQUESTS_DETAIL_SEMAPHORE_WAIT_OP
#define BOOST_REQUESTS_DETAIL_SEMAPHORE_WAIT_OP

#include <boost/requests/detail/bilist_node.hpp>
#include <boost/system/error_code.hpp>

namespace boost::requests
{
struct async_semaphore_base;

namespace detail
{
struct semaphore_wait_op : detail::bilist_node
{
    inline semaphore_wait_op(async_semaphore_base *host);

    virtual void complete(system::error_code) = 0;

    async_semaphore_base *host_;
};

}   // namespace detail
}   // namespace boost::requests

#endif

#include <boost/requests/detail/impl/semaphore_wait_op.hpp>