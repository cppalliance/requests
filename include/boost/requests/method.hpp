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

template<typename Stream>
struct basic_connection;

using empty = beast::http::empty_body::value_type;

template<typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req,
         system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection>
auto head(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req = {}) -> response_base
{
  auto s = conn.ropen(http::verb::head, target, empty{}, std::move(req));
  return {std::move(s).headers(), std::move(s).history()};
}

template<typename Connection>
auto head(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req,
          system::error_code & ec) -> response_base
{
  auto s = conn.ropen(http::verb::head, target, empty{}, std::move(req), ec);
  return {std::move(s).headers(), std::move(s).history()};
}

template<typename Connection, typename RequestBody>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response
{
    auto s = conn.ropen(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req));
    response res{req.get_allocator()};
    s.read(res.buffer);
    res.headers = std::move(s).headers();
    res.history = std::move(s).history();
    return res;
}

template<typename Connection, typename RequestBody>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::patch, target, empty{}, std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req,
          system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::patch, target, empty{}, std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection, typename RequestBody>
auto patch(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection, typename RequestBody>
auto patch(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req,
           system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection, typename RequestBody>
auto put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection, typename RequestBody>
auto put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection>
auto delete_(Connection & conn,
           urls::url_view target,
           typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::delete_, target, empty{}, std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return s;
}

template<typename Connection>
auto delete_(Connection & conn,
           urls::url_view target,
           typename Connection::request_type req,
           system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::delete_, target, empty{}, std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection, typename RequestBody>
auto delete_(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}

template<typename Connection, typename RequestBody>
auto delete_(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req,
           system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Stream>
auto connect(basic_connection<Stream> & conn,
             typename basic_connection<Stream>::target_view target,
             typename basic_connection<Stream>::request_type req = {}) -> response_base
{
  auto s = conn.ropen(http::verb::connect, target, empty{}, std::move(req));
  return {std::move(s).headers(), std::move(s).history()};
}

template<typename Stream>
auto connect(basic_connection<Stream> & conn,
             typename basic_connection<Stream>::target_view target,
             typename basic_connection<Stream>::request_type req,
             system::error_code & ec) -> response_base
{
  auto s = conn.ropen(http::verb::connect, target, empty{}, std::move(req), ec);
  return {std::move(s).headers(), std::move(s).history()};
}


template<typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {}) -> response
{
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req));
  response res{req.get_allocator()};
  s.read(res.buffer);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return s;
}

template<typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response
{
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req), ec);
  response res{req.get_allocator()};
  if (!ec)
    s.read(res.buffer, ec);
  res.headers = std::move(s).headers();
  res.history = std::move(s).history();
  return res;
}


template<typename Connection>
auto trace(Connection & conn,
           urls::url_view target,
           typename Connection::request_type req = {}) -> response_base
{
  auto s = conn.ropen(http::verb::trace, target, empty{}, std::move(req));
  return {std::move(s).headers(), std::move(s).history()};
}

template<typename Connection>
auto trace(Connection & conn,
           urls::url_view target,
           typename Connection::request_type req,
           system::error_code & ec) -> response_base
{
  auto s = conn.ropen(http::verb::trace, target, empty{}, std::move(req));
  return {std::move(s).headers(), std::move(s).history()};
}

template<typename Connection,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_get(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::get, target, empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_head(Connection & conn,
           urls::url_view target,
           typename Connection::request_type req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::head, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,
         typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_post(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::post, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,
         typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req  = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::put, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,
         typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_patch(Connection & conn,
            urls::url_view target,
            RequestBody && request_body,
            typename Connection::request_type req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::patch, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,
         typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(Connection & conn,
             urls::url_view target,
             RequestBody && request_body,
             typename Connection::request_type req = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::delete_, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<typename Connection,
         typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req  = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::delete_, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename basic_connection<Stream>::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_connect(basic_connection<Stream> & conn,
              typename basic_connection<Stream>::target_view target,
              typename basic_connection<Stream>::request_type req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename basic_connection<Stream>::executor_type))
{
  return async_request(conn, http::verb::connect, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_options(Connection & conn,
              urls::url_view target,
              typename Connection::request_type req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::options, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_trace(Connection & conn,
            urls::url_view target,
            typename Connection::request_type req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return async_request(conn, http::verb::trace, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


// -------------------------------------- default session stuff --------------------------------------

inline auto get(urls::url_view target, http::fields req = {}) -> response
{
  return request(http::verb::get, target, empty{}, std::move(req));
}


inline auto get(urls::url_view target,
                http::fields req,
                system::error_code & ec) -> response
{
  return request(http::verb::get, target, empty{}, std::move(req), ec);
}


inline auto head(
    urls::url_view target,
    http::fields req = {}) -> response
{
  return request(http::verb::head, target, empty{}, std::move(req));
}

inline auto head(
    urls::url_view target,
    http::fields req,
    system::error_code & ec) -> response
{
  return request(http::verb::head, target, empty{}, std::move(req), ec);
}

template<typename RequestBody>
auto post(
    urls::url_view target,
    RequestBody && request_body,
    http::fields req = {}) -> response
{
  return request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto post(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          system::error_code & ec) -> response
{
  return request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req = {}) -> response
{
  return request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req,
         system::error_code & ec) -> response
{
  return request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req = {}) -> response
{
  return request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           system::error_code & ec) -> response
{
  return request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}

template<typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req = {}) -> response
{
  return request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


inline auto delete_(urls::url_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::delete_, target, empty{}, std::move(req));
}

inline auto delete_(urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::delete_, target, empty{}, std::move(req), ec);
}


inline auto options(urls::url_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::options, target, empty{}, std::move(req));
}

inline auto options(urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::options, target, empty{}, std::move(req), ec);
}


inline auto trace(urls::url_view target,
           http::fields req = {}) -> response
{
  return request(http::verb::trace, target, empty{}, std::move(req));
}

inline auto trace(urls::url_view target,
           http::fields req,
           system::error_code & ec) -> response
{
  return request(http::verb::trace, target, empty{}, std::move(req), ec);
}

template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
async_get(urls::url_view target,
          http::fields req,
          CompletionToken && completion_token)
{
  return async_request(http::verb::get, target, empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
async_head(urls::url_view target,
           http::fields req,
           CompletionToken && completion_token)
{
  return async_request(http::verb::head, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename RequestBody,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_post(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           CompletionToken && completion_token)
{
  return async_request(http::verb::post, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_put(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          CompletionToken && completion_token)
{
  return async_request(http::verb::put, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_patch(urls::url_view target,
            RequestBody && request_body,
            http::fields req,
            CompletionToken && completion_token)
{
  return async_request(http::verb::patch, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             CompletionToken && completion_token)
{
  return async_request(http::verb::delete_, target,
                       std::forward<RequestBody>(request_body), std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(urls::url_view target,
             http::fields req,
             CompletionToken && completion_token)
{
  return async_request(http::verb::delete_, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_options(urls::url_view target,
              http::fields req,
              CompletionToken && completion_token)
{
  return async_request(http::verb::options, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_trace(urls::url_view target,
            http::fields req,
            CompletionToken && completion_token)
{
  return async_request(http::verb::trace, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

}
}

#endif // BOOST_REQUESTS_METHOD_HPP
