// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/requests/detail/cache_service.hpp>
#include "doctest.h"

TEST_CASE("http-cache-service")
{
    using namespace boost;

    boost::asio::io_context ctx;
    auto & cache = use_service<requests::detail::implicit_session_service>(ctx);
}


