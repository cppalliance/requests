// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_DETAIL_VARIADIC_HPP
#define BOOST_REQUESTS_DETAIL_VARIADIC_HPP

#include <boost/asio/async_result.hpp>
#include <boost/mp11/algorithm.hpp>
#include <tuple>

namespace boost {
namespace requests {
namespace detail {

template<typename Signature, typename CompletionHandler, typename = void>
struct is_completion_token : std::false_type {};

template<typename Signature, typename CompletionHandler>
struct is_completion_token<Signature, CompletionHandler,
        decltype(boost::asio::async_initiate<CompletionHandler, Signature>(
                boost::asio::detail::initiation_archetype<Signature>{},
                std::declval<CompletionHandler&>()))
        >  : std::true_type {};

template<typename Signature, typename Executor, typename ... Ts>
using completion_token = std::conditional_t<
        is_completion_token<Signature, mp11::mp_back<mp11::mp_list<void, Ts&&...>>>::value,
        mp11::mp_back<mp11::mp_list<void, Ts...>>,
        asio::default_completion_token_t<Executor>>;

template<typename Signature, typename Executor, typename ... Ts>
asio::default_completion_token_t<Executor> get_completion_token_impl(std::false_type, Ts && ...)
{
    return asio::default_completion_token_t<Executor>();
}

template<typename Signature, typename Executor, typename ... Ts>
auto get_completion_token_impl(std::true_type, Ts && ... ts) -> mp11::mp_back<mp11::mp_list<void, Ts...>>
{
    return std::forward<mp11::mp_back<mp11::mp_list<void, Ts...>>>(std::get<sizeof...(Ts) -1u>(std::tie(ts...)));
}

template<typename Signature, typename Executor, typename ... Ts>
completion_token<Signature, Executor, Ts...> get_completion_token(Ts && ... ts)
{
    return get_completion_token_impl<Signature, Executor>(
            is_completion_token<Signature, mp11::mp_back<mp11::mp_list<void, Ts&&...>>>{},
            std::forward<Ts>(ts)...);
}


template<typename Signature, typename Executor, typename ... Ts>
using tie_args_result = std::conditional_t<
        is_completion_token<Signature, mp11::mp_back<mp11::mp_list<void, Ts&&...>>>::value,
        mp11::mp_back<mp11::mp_list<void, Ts...>>,
        asio::default_completion_token_t<Executor>>;

template<typename Signature, typename Executor>
auto tie_args() -> std::tuple<>
{
    return std::tuple();
}

template<typename Tuple, std::size_t ... Idx>
auto tie_impl(Tuple && tup, std::index_sequence<Idx...>) -> std::tuple<std::tuple_element_t<Idx, Tuple> ...>
{
    return std::tuple<std::tuple_element_t<Idx, Tuple> ...>(std::get<Idx>(std::move(tup))...);
}

template<typename Signature, typename Executor, typename ... Ts>
auto tie_args(Ts && ... ts)
   /* -> std::conditional_t<
        is_completion_token<Signature, mp11::mp_back<mp11::mp_list<void, Ts&&...>>>::value,
        mp11::mp_pop_back<std::tuple<Ts&&...>>,
        std::tuple<Ts&&...>>
        */
{
    using seq = std::make_index_sequence<
            is_completion_token<Signature, mp11::mp_back<mp11::mp_list<void, Ts&&...>>>::value ?
                sizeof...(Ts) - 1 : sizeof...(Ts)>;

    return tie_impl(std::tuple<Ts&&...>(std::forward<Ts>(ts)...), seq{});
}



}
}
}

#endif //BOOST_REQUESTS_DETAIL_VARIADIC_HPP
