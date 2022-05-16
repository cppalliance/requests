//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef BOOST_REQUESTS_IMPL_BASIC_ASYNC_SEMAPHORE_HPP
#define BOOST_REQUESTS_IMPL_BASIC_ASYNC_SEMAPHORE_HPP

#include <boost/asio/post.hpp>
#include <boost/requests/async_semaphore.hpp>
#include <boost/requests/detail/semaphore_wait_op_model.hpp>
#include <boost/system/error_code.hpp>

namespace boost::requests
{
template < class Executor >
basic_async_semaphore< Executor >::basic_async_semaphore(executor_type exec,
                                                         int initial_count)
: async_semaphore_base(initial_count)
, exec_(std::move(exec))
{
}

template < class Executor >
typename basic_async_semaphore< Executor >::executor_type const &
basic_async_semaphore< Executor >::get_executor() const
{
    return exec_;
}

template < class Executor >
template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code)) CompletionHandler >
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(system::error_code))
basic_async_semaphore< Executor >::async_acquire(CompletionHandler &&token)
{
    return asio::async_initiate< CompletionHandler, void(system::error_code) >(
        [this]< class Handler >(Handler &&handler)
        {
            auto e = get_associated_executor(handler, get_executor());
            if (count())
            {
                decrement();

                asio::post(std::move(e),
                           asio::experimental::append(
                               std::forward< Handler >(handler), system::error_code()));
                return;
            }

            using handler_type = std::decay_t< Handler >;
            using model_type =
                detail::semaphore_wait_op_model< decltype(e), handler_type >;
            model_type *model = model_type ::construct(
                this, std::move(e), std::forward< Handler >(handler));
            try
            {
                add_waiter(model);
            }
            catch (...)
            {
                model_type::destroy(model);
                throw;
            }
        },
        token);
}



template < class Executor >
template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code)) CompletionHandler >
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(system::error_code))
basic_async_semaphore< Executor >::async_recursive_acquire(CompletionHandler &&token)
{
    return asio::async_initiate< CompletionHandler, void(system::error_code) >(
           [this]< class Handler >(Handler &&handler)
    {
        auto e = get_associated_executor(handler, get_executor());
        if (count())
        {
            decrement();
            std::forward< Handler >(handler)(system::error_code());
            return;
        }

        using handler_type = std::decay_t< Handler >;
        using model_type =
        detail::semaphore_wait_op_model< decltype(e), handler_type >;
        model_type *model = model_type ::construct(
                this, std::move(e), std::forward< Handler >(handler));
        try
        {
            add_waiter(model);
        }
        catch (...)
        {
            model_type::destroy(model);
            throw;
        }
    },
    token);
}


}   // namespace boost::requests

#endif
