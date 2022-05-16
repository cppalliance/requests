// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef REQUESTS_CACHE_SERVICE_HPP
#define REQUESTS_CACHE_SERVICE_HPP

#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/unordered_map.hpp>


namespace boost::requests::detail
{

template<typename ExecutionContext>
struct basic_cache_service : asio::execution_context::service
{
    inline static asio::execution_context::id id{};

    explicit basic_cache_service(ExecutionContext& context,
                                      typename asio::constraint<asio::is_convertible<ExecutionContext&, asio::execution_context&>::value >::type = 0)
        : asio::execution_context::service(context)
    {
    }

    using socket_type = asio::basic_socket<asio::ip::tcp, typename ExecutionContext::excutor_type>;

    boost::unordered_multimap<std::string, socket_type> http_sessions;
    boost::unordered_multimap<std::string, asio::ssl::stream<socket_type>> https_sessions;

    void shutdown()
    {
        http_sessions.clear();
        https_sessions.clear();
    }
};

}

#endif //REQUESTS_CACHE_SERVICE_HPP
