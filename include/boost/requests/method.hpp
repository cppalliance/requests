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

using empty = beast::http::empty_body::value_type;

template<typename Connection>
auto get(Connection & conn,
         typename Connection::target_view target,
         typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::get, target, empty{}, std::move(req));
}


template<typename Connection>
auto get(Connection & conn,
         typename Connection::target_view target,
         typename Connection::request_type req,
         system::error_code & ec) -> response
{
  return conn.request(http::verb::get, target, empty{}, std::move(req), ec);
}


template<typename Connection>
auto head(Connection & conn,
          typename Connection::target_view target,
          typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::head, target, empty{}, std::move(req));
}

template<typename Connection>
auto head(Connection & conn,
          typename Connection::target_view target,
          typename Connection::request_type req,
          system::error_code & ec) -> response
{
  return conn.request(http::verb::head, target, empty{}, std::move(req), ec);
}

template<typename Connection, typename RequestBody>
auto post(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Connection, typename RequestBody>
auto post(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response
{
  return conn.request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Connection, typename RequestBody>
auto put(Connection & conn,
         typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Connection, typename RequestBody>
auto put(Connection & conn,
         typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         system::error_code & ec) -> response
{
  return conn.request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Connection, typename RequestBody>
auto patch(Connection & conn,
         typename Connection::target_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Connection, typename RequestBody>
auto patch(Connection & conn,
           typename Connection::target_view target,
           RequestBody && request_body,
           typename Connection::request_type req,
           system::error_code & ec) -> response
{
  return conn.request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}

template<typename Connection, typename RequestBody>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             RequestBody && request_body,
             typename Connection::request_type req = {})
    -> typename std::enable_if_t<std::is_same<std::decay_t<RequestBody>, typename Connection::request_type>::value,  response>::type
{
  return conn.request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Connection, typename RequestBody>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             RequestBody && request_body,
             typename Connection::request_type req,
             system::error_code & ec)
    -> typename std::enable_if_t<std::is_same<std::decay_t<RequestBody>, typename Connection::request_type>::value,  response>::type
{
  return conn.request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Connection>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::delete_, target, empty{}, std::move(req));
}

template<typename Connection>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::delete_, target, empty{}, std::move(req), ec);
}


template<typename Connection>
auto connect(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::connect, target, empty{}, std::move(req));
}

template<typename Connection>
auto connect(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::connect, target, empty{}, std::move(req), ec);
}


template<typename Connection, typename RequestBody>
auto options(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::options, target, empty{}, std::move(req));
}

template<typename Connection, typename RequestBody>
auto options(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::options, target, empty{}, std::move(req), ec);
}


template<typename Connection, typename RequestBody>
auto trace(Connection & conn,
           typename Connection::target_view target,
           typename Connection::request_type req = {}) -> response
{
  return conn.request(http::verb::trace, target, empty{}, std::move(req));
}

template<typename Connection, typename RequestBody>
auto trace(Connection & conn,
           typename Connection::target_view target,
           typename Connection::request_type req,
           system::error_code & ec) -> response
{
  return conn.request(http::verb::trace, target, empty{}, std::move(req), ec);
}


template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_get(Connection & conn,
          typename Connection::target_view target,
          typename Connection::request_type req = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::get, target, empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_head(Connection & conn,
           typename Connection::target_view target,
           typename Connection::request_type req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::head, target,
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
           typename Connection::target_view target,
           RequestBody && request_body,
           typename Connection::request_type req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::post, target,
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
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req  = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::put, target,
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
            typename Connection::target_view target,
            RequestBody && request_body,
            typename Connection::request_type req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::patch, target,
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
             typename Connection::target_view target,
             RequestBody && request_body,
             typename Connection::request_type req = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::delete_, target,
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
             typename Connection::target_view target,
             typename Connection::request_type req  = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::delete_, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<typename Connection,  
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE( typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_connect(Connection & conn,
              typename Connection::target_view target,
              typename Connection::request_type req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::connect, target,
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
              typename Connection::target_view target,
              typename Connection::request_type req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::options, target,
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
            typename Connection::target_view target,
            typename Connection::request_type req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  return conn.async_request(http::verb::trace, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

// -------------------------------------- basic session stuff --------------------------------------

template<typename Executor>
struct basic_session;

template<typename Executor>
auto get(basic_session<Executor> & conn,
         urls::url_view target,
         http::fields req = {}) -> response
{
  return conn.request(http::verb::get, target, empty{}, std::move(req));
}


template<typename Executor>
auto get(basic_session<Executor> & conn,
         urls::url_view target,
         http::fields req,
         system::error_code & ec) -> response
{
  return conn.request(http::verb::get, target, empty{}, std::move(req), ec);
}


template<typename Executor>
auto head(basic_session<Executor> & conn,
          urls::url_view target,
          http::fields req = {}) -> response
{
  return conn.request(http::verb::head, target, empty{}, std::move(req));
}

template<typename Executor>
auto head(basic_session<Executor> & conn,
          urls::url_view target,
          http::fields req,
          system::error_code & ec) -> response
{
  return conn.request(http::verb::head, target, empty{}, std::move(req), ec);
}

template<typename Executor, typename RequestBody>
auto post(basic_session<Executor> & conn,
          urls::url_view target,
          RequestBody && request_body,
          http::fields req = {}) -> response
{
  return conn.request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Executor, typename RequestBody>
auto post(basic_session<Executor> & conn,
          urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          system::error_code & ec) -> response
{
  return conn.request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto put(basic_session<Executor> & conn,
         urls::url_view target,
         RequestBody && request_body,
         http::fields req = {}) -> response
{
  return conn.request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Executor, typename RequestBody>
auto put(basic_session<Executor> & conn,
         urls::url_view target,
         RequestBody && request_body,
         http::fields req,
         system::error_code & ec) -> response
{
  return conn.request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto patch(basic_session<Executor> & conn,
           urls::url_view target,
           RequestBody && request_body,
           http::fields req = {}) -> response
{
  return conn.request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Executor, typename RequestBody>
auto patch(basic_session<Executor> & conn,
           urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           system::error_code & ec) -> response
{
  return conn.request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}

template<typename Executor, typename RequestBody>
auto delete_(basic_session<Executor> & conn,
             urls::url_view target,
             RequestBody && request_body,
             http::fields req = {}) -> response
{
  return conn.request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Executor, typename RequestBody>
auto delete_(basic_session<Executor> & conn,
             urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto delete_(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req = {}) -> response
{
  return conn.request(http::verb::delete_, target, empty{}, std::move(req));
}

template<typename Executor, typename RequestBody>
auto delete_(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::delete_, target, empty{}, std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto connect(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req = {}) -> response
{
  return conn.request(http::verb::connect, target, empty{}, std::move(req));
}

template<typename Executor, typename RequestBody>
auto connect(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::connect, target, empty{}, std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto options(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req = {}) -> response
{
  return conn.request(http::verb::options, target, empty{}, std::move(req));
}

template<typename Executor, typename RequestBody>
auto options(basic_session<Executor> & conn,
             urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return conn.request(http::verb::options, target, empty{}, std::move(req), ec);
}


template<typename Executor, typename RequestBody>
auto trace(basic_session<Executor> & conn,
           urls::url_view target,
           http::fields req = {}) -> response
{
  return conn.request(http::verb::trace, target, empty{}, std::move(req));
}

template<typename Executor, typename RequestBody>
auto trace(basic_session<Executor> & conn,
           urls::url_view target,
           http::fields req,
           system::error_code & ec) -> response
{
  return conn.request(http::verb::trace, target, empty{}, std::move(req), ec);
}


template<typename Executor,  
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_get(basic_session<Executor> & conn, 
          urls::url_view target, 
          http::fields req = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::get, target, empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

template<typename Executor,  
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_head(basic_session<Executor> & conn, urls::url_view target, 
           http::fields req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::head, target,
                            empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

template<typename Executor,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_post(basic_session<Executor> & conn, urls::url_view target, 
           RequestBody && request_body,
           http::fields req  = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::post, target,
                            std::forward<RequestBody>(request_body), std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

template<typename Executor,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_put(basic_session<Executor> & conn, urls::url_view target, 
          RequestBody && request_body,
          http::fields req  = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::put, target,
                            std::forward<RequestBody>(request_body), std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

template<typename Executor,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_patch(basic_session<Executor> & conn, urls::url_view target, 
            RequestBody && request_body,
            http::fields req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::patch, target,
                            std::forward<RequestBody>(request_body), std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

template<typename Executor,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(basic_session<Executor> & conn, urls::url_view target, 
             RequestBody && request_body,
             http::fields req = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::delete_, target,
                            std::forward<RequestBody>(request_body), std::move(req),
                            std::forward<CompletionToken>(completion_token));
}


template<typename Executor,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_delete(basic_session<Executor> & conn, urls::url_view target, 
             http::fields req  = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::delete_, target,
                            empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}


template<typename Executor,  
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_connect(basic_session<Executor> & conn, urls::url_view target, 
              http::fields req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::connect, target,
                            empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}


template<typename Executor,  
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_options(basic_session<Executor> & conn, urls::url_view target, 
              http::fields req  = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::options, target,
                            empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}


template<typename Executor,  
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_trace(basic_session<Executor> & conn, urls::url_view target, 
            http::fields req  = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
  return conn.async_request(http::verb::trace, target,
                            empty{}, std::move(req),
                            std::forward<CompletionToken>(completion_token));
}

// -------------------------------------- default session stuff --------------------------------------

inline auto get(core::string_view target, http::fields req = {}) -> response
{
  return request(http::verb::get, target, empty{}, std::move(req));
}


inline auto get(core::string_view target,
                http::fields req,
                system::error_code & ec) -> response
{
  return request(http::verb::get, target, empty{}, std::move(req), ec);
}


inline auto head(
    core::string_view target,
    http::fields req = {}) -> response
{
  return request(http::verb::head, target, empty{}, std::move(req));
}

inline auto head(
    core::string_view target,
    http::fields req,
    system::error_code & ec) -> response
{
  return request(http::verb::head, target, empty{}, std::move(req), ec);
}

template<typename RequestBody>
auto post(
    core::string_view target,
    RequestBody && request_body,
    http::fields req = {}) -> response
{
  return request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto post(core::string_view target,
          RequestBody && request_body,
          http::fields req,
          system::error_code & ec) -> response
{
  return request(http::verb::post, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename RequestBody>
auto put(core::string_view target,
         RequestBody && request_body,
         http::fields req = {}) -> response
{
  return request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto put(core::string_view target,
         RequestBody && request_body,
         http::fields req,
         system::error_code & ec) -> response
{
  return request(http::verb::put, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


template<typename RequestBody>
auto patch(core::string_view target,
           RequestBody && request_body,
           http::fields req = {}) -> response
{
  return request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto patch(core::string_view target,
           RequestBody && request_body,
           http::fields req,
           system::error_code & ec) -> response
{
  return request(http::verb::patch, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}

template<typename RequestBody>
auto delete_(core::string_view target,
             RequestBody && request_body,
             http::fields req = {}) -> response
{
  return request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename RequestBody>
auto delete_(core::string_view target,
             RequestBody && request_body,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::delete_, target, std::forward<RequestBody>(request_body), std::move(req), ec);
}


inline auto delete_(core::string_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::delete_, target, empty{}, std::move(req));
}

inline auto delete_(core::string_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::delete_, target, empty{}, std::move(req), ec);
}


inline auto connect(core::string_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::connect, target, empty{}, std::move(req));
}

inline auto connect(core::string_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::connect, target, empty{}, std::move(req), ec);
}


inline auto options(core::string_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::options, target, empty{}, std::move(req));
}

inline auto options(core::string_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::options, target, empty{}, std::move(req), ec);
}


inline auto trace(core::string_view target,
           http::fields req = {}) -> response
{
  return request(http::verb::trace, target, empty{}, std::move(req));
}

inline auto trace(core::string_view target,
           http::fields req,
           system::error_code & ec) -> response
{
  return request(http::verb::trace, target, empty{}, std::move(req), ec);
}

template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
async_get(core::string_view target,
          http::fields req,
          CompletionToken && completion_token)
{
  return async_request(http::verb::get, target, empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
async_head(core::string_view target,
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
async_post(core::string_view target,
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
async_put(core::string_view target,
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
async_patch(core::string_view target,
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
async_delete(core::string_view target,
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
async_delete(core::string_view target,
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
async_connect(core::string_view target,
              http::fields req,
              CompletionToken && completion_token)
{
  return async_request(http::verb::connect, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_options(core::string_view target,
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
async_trace(core::string_view target,
            http::fields req,
            CompletionToken && completion_token)
{
  return async_request(http::verb::trace, target,
                       empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}


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


inline auto connect(urls::url_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::connect, target, empty{}, std::move(req));
}

inline auto connect(urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(http::verb::connect, target, empty{}, std::move(req), ec);
}


inline auto options(urls::url_view target,
             http::fields req = {}) -> response
{
  return request(http::verb::options, target, empty{}, std::move(req));
}

inline auto options(
    urls::url_view target,
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

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
async_get(urls::url_view target,
          http::fields req,
          CompletionToken && completion_token)
{
  return async_request(http::verb::get, target, empty{}, std::move(req),
                       std::forward<CompletionToken>(completion_token));
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, response))
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
async_connect(urls::url_view target,
              http::fields req,
              CompletionToken && completion_token)
{
  return async_request(http::verb::connect, target,
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
