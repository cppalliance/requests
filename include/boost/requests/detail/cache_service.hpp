// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef REQUESTS_CACHE_SERVICE_HPP
#define REQUESTS_CACHE_SERVICE_HPP

#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>

namespace boost::requests::detail
{

struct implicit_session_service : asio::execution_context::service
{
    inline static asio::execution_context::id id{};

        template <typename ExecutionContext>
    explicit implicit_session_service(ExecutionContext& context,
                                      typename asio::constraint<asio::is_convertible<ExecutionContext&, asio::execution_context&>::value >::type = 0)
        : asio::execution_context::service(context)
    {
    }

    void shutdown()
    {
    }
};

}

#endif //REQUESTS_CACHE_SERVICE_HPP
