//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_EXECUTOR_WITH_DEFAULT_HPP
#define BOOST_REQUESTS_DETAIL_EXECUTOR_WITH_DEFAULT_HPP

#include <type_traits>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename ... Args>
using void_t = void;

template<typename Executor, typename = void>
struct strip_executor_with_default : std::false_type
{
  using type = Executor;
};

template<template<class> class Template, typename Executor>
struct strip_executor_with_default<Template<Executor>,
                                   void_t<typename Template<Executor>::default_completion_token_type>>
    : std::is_base_of<Executor, Template<Executor>>
{
  using type = Executor;
};

template<typename Executor>
using strip_executor_with_default_t = typename strip_executor_with_default<Executor>::type;

}
}
}

#endif // BOOST_REQUESTS_DETAIL_EXECUTOR_WITH_DEFAULT_HPP
