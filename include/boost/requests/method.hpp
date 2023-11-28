//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOST_REQUESTS_METHOD_HPP
#define BOOST_REQUESTS_METHOD_HPP

#include <boost/system/error_code.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/request.hpp>
#include <boost/requests/response.hpp>

namespace boost
{
namespace requests
{

constexpr bind_empty_request<   http::verb::get>     get;
constexpr bind_empty_request<   http::verb::head>    head;
constexpr bind_request<         http::verb::post>    post;
constexpr bind_optional_request<http::verb::patch>   patch;
constexpr bind_request<         http::verb::put>     put;
constexpr bind_optional_request<http::verb::delete_> delete_;
constexpr bind_empty_request<   http::verb::connect> connect;
constexpr bind_empty_request<   http::verb::options> options;
constexpr bind_request<         http::verb::trace>   trace;


constexpr bind_empty_async_request<   http::verb::get>     async_get;
constexpr bind_empty_async_request<   http::verb::head>    async_head;
constexpr bind_async_request<         http::verb::post>    async_post;
constexpr bind_optional_async_request<http::verb::patch>   async_patch;
constexpr bind_async_request<         http::verb::put>     async_put;
constexpr bind_optional_async_request<http::verb::delete_> async_delete;
constexpr bind_empty_async_request<   http::verb::connect> async_connect;
constexpr bind_empty_async_request<   http::verb::options> async_options;
constexpr bind_async_request<         http::verb::trace>   async_trace;

}
}

#endif // BOOST_REQUESTS_METHOD_HPP
