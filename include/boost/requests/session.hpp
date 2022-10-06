// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_BASIC_SESSION_HPP
#define BOOST_REQUESTS_BASIC_SESSION_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/requests/connection.hpp>
#include <boost/beast/http/message.hpp>

namespace boost::requests
{

template<typename Executor = asio::any_io_executor>
struct basic_session
{
    /// The type of the executor associated with the object.
    typedef Executor executor_type;

    /// Rebinds the timer type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The timer type when rebound to the specified executor.
        typedef basic_session<Executor1> other;
    };

    /// Constructor.
    explicit basic_session(const executor_type &ex)
            : executor_(ex)
    {
    }

    /// Constructor.
    template<typename ExecutionContext>
    explicit basic_session(ExecutionContext &context,
                     typename asio::constraint<
                                   asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                           >::type = 0)
            : executor_(context.get_executor())
    {
    }

    /// Get the executor associated with the object.
    executor_type get_executor() BOOST_ASIO_NOEXCEPT
    {
        return executor_;
    }



  private:
    executor_type executor_;
    //boost::unordered_multimap<std::string, basic_http_connection<Executor>> http_connections_;
    //boost::unordered_multimap<std::string, basic_https_connection<Executor>> https_connections_;
};

typedef basic_session<> session;

}

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
