//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_REQUEST_HPP
#define BOOST_REQUESTS_REQUEST_HPP

#include <boost/requests/http.hpp>
#include <boost/requests/service.hpp>
#include <boost/requests/session.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename Connection, typename RequestBody = empty>
struct async_request_op;

}

template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             typename Connection::request_type req,
             system::error_code & ec) -> response;

template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             typename Connection::request_type req)
    -> response
{
  boost::system::error_code ec;
  auto res = request(conn, method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec, "request"));
  return res;
}




template<typename RequestBody>
auto request(beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(default_session(), method, path, std::forward<RequestBody>(body), std::move(req), ec);
}

template<typename RequestBody>
auto request(beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             http::fields req)
    -> response
{
  boost::system::error_code ec;
  auto res = request(method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec, "request"));
  return res;
}


template<typename RequestBody>
auto request(beast::http::verb method,
             core::string_view path,
             RequestBody && body,
             http::fields req)
    -> response
{
  boost::system::error_code ec;
  auto res = request(method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec, "request"));
  return res;
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
async_request(beast::http::verb method,
              core::string_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token);

template<typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR( void (system::error_code, response)) CompletionToken
           = typename Connection::default_token>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, response))
async_request(Connection & conn,
              beast::http::verb method,
              urls::url_view target,
              RequestBody && body,
              typename Connection::request_type req,
              CompletionToken && completion_token = typename Connection::default_token());
}
}

#include <boost/requests/impl/request.hpp>

#endif // BOOST_REQUESTS_REQUEST_HPP
