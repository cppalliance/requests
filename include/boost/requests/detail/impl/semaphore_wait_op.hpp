//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef BOOST_REQUESTS_DETAIL_IMPL_SEMAPHORE_WAIT_OP
#define BOOST_REQUESTS_DETAIL_IMPL_SEMAPHORE_WAIT_OP

#include <boost/requests/detail/semaphore_wait_op.hpp>

namespace boost::requests
{
namespace detail
{
semaphore_wait_op::semaphore_wait_op(async_semaphore_base *host)
: host_(host)
{
}
}   // namespace detail
}   // namespace boost::requests

#endif
