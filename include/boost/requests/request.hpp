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

#include <boost/beast/http/verb.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename Connection>
using target_view =
    std::conditional_t<
        std::is_base_of<Connection, session>::value,
        urls::url_view, urls::pct_string_view>;

template<typename Connection>
using request_type =
    std::conditional_t<
        std::is_base_of<Connection, session>::value,
        http::headers, request_parameters>;

BOOST_REQUESTS_DECL
std::pair<stream, history> 
request_stream_impl(
    connection & conn,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    system::error_code & ec);


BOOST_REQUESTS_DECL
std::pair<stream, history>
request_stream_impl(
    connection_pool & pool,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    system::error_code & ec);

BOOST_REQUESTS_DECL
std::pair<stream, history>
request_stream_impl(
    session & sess,
    http::verb method,
    urls::url_view path,
    source_ptr source,
    http::headers headers,
    system::error_code & ec);



BOOST_REQUESTS_DECL
void async_request_stream_impl(
      connection & conn,
      http::verb method,
      urls::pct_string_view path,
      source_ptr source,
      request_parameters req,
      asio::any_completion_handler<void(error_code, stream, history)> handler);

BOOST_REQUESTS_DECL
void async_request_stream_impl(
      connection_pool & pool,
      http::verb method,
      urls::pct_string_view path,
      source_ptr source,
      request_parameters req,
      asio::any_completion_handler<void(error_code, stream, history)> handler);

BOOST_REQUESTS_DECL
void async_request_stream_impl(
      session & sess,
      http::verb method,
      urls::url_view path,
      source_ptr source,
      http::headers headers,
      asio::any_completion_handler<void(error_code, stream, history)> handler);

}


// connection
template<typename Connection, typename RequestBody>
auto request_stream(
    Connection & conn,
    http::verb method,
    detail::target_view<Connection> path, // pct_string_view when connection
    RequestBody && body,
    detail::request_type<Connection> req, // headers for conn,
    system::error_code & ec) -> std::pair<stream, history>
{
  auto src = make_source(std::forward<RequestBody>(body));
  return detail::request_stream_impl(conn, method, path, std::move(src), std::move(req), ec);
}


template<typename Connection, typename RequestBody>
auto request_stream(
    Connection & conn,
    http::verb method,
    detail::target_view<Connection> path, // pct_string_view when connection
    RequestBody && body,
    detail::request_type<Connection> req /* headers for conn*/ ) -> std::pair<stream, history>
{
  boost::system::error_code ec;
  auto res = request_stream(conn, method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec, "request_stream"));
  return res;
}


template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream, history)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream, history))
async_request_stream(
    http::verb method,
    core::string_view path,
    RequestBody && body,
    http::fields req,
    CompletionToken && completion_token);

template<typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR( void (system::error_code, stream, history)) CompletionToken
          = typename Connection::default_token>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, stream, history))
async_request_stream(
    Connection & conn,
    http::verb method,
    detail::target_view<Connection> path,
    RequestBody && body,
    detail::request_type<Connection> req,
    CompletionToken && completion_token = typename Connection::default_token())
{

  return asio::async_initiate<CompletionToken, void (system::error_code, stream, history)>(
      [](asio::any_completion_handler<void(error_code, stream, history)> handler,
         Connection & conn,
         http::verb method,
         detail::target_view<Connection> path,
         RequestBody && body,
         detail::request_type<Connection> req)
      {
        detail::async_request_stream_impl(conn, method, path,
                                          make_source(std::forward<RequestBody>(body)),
                                          std::move(req), std::move(handler));
      },
      completion_token,
      std::ref(conn), method, std::move(path), std::forward<RequestBody>(body), std::move(req));
}


template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             http::verb method,
             urls::url_view path,
             RequestBody && body,
             detail::request_type<Connection> req,
             system::error_code & ec) -> response;

template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             http::verb method,
             urls::url_view path,
             RequestBody && body,
             detail::request_type<Connection> req)
    -> response
{
  boost::system::error_code ec;
  auto res = request(conn, method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec, "request"));
  return res;
}


template<typename RequestBody>
auto request(http::verb method,
             urls::url_view path,
             RequestBody && body,
             http::fields req,
             system::error_code & ec) -> response
{
  return request(default_session(), method, path, std::forward<RequestBody>(body), std::move(req), ec);
}

template<typename RequestBody>
auto request(http::verb method,
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

template<typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (error_code, response)) CompletionToken
            = typename Connection::default_token>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (error_code, response))
async_request(Connection & conn,
              http::verb method,
              detail::target_view<Connection> target,
              RequestBody && body,
              detail::request_type<Connection> req,
              CompletionToken && completion_token = typename Connection::default_token());

template<typename RequestBody,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (error_code, response))
async_request(http::verb method,
              urls::url_view target,
              RequestBody && body,
              http::headers req,
              CompletionToken && completion_token);

template<http::verb Method>
struct bind_request
{
  template<typename Connection, typename RequestBody>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  RequestBody && request_body,
                  detail::request_type<Connection> req = {}) const -> response
  {
    return request(conn, Method, target, std::forward<RequestBody>(request_body), std::move(req));
  };

  template<typename Connection, typename RequestBody>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  RequestBody && request_body,
                  detail::request_type<Connection> req,
                  system::error_code & ec) const -> response
  {
    return request(conn, Method, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  }


  template<typename RequestBody>
  auto operator()(urls::url_view target,
                  RequestBody && request_body,
                  http::fields req = {}) const -> response
  {
    return request(Method, target, std::forward<RequestBody>(request_body), std::move(req));
  };

  template<typename RequestBody>
  auto operator()(urls::url_view target,
                  RequestBody && request_body,
                  http::fields  req,
                  system::error_code & ec) const -> response
  {
    return request(Method, target, std::forward<RequestBody>(request_body), std::move(req), ec);
  }
};


template<http::verb Method>
struct bind_empty_request
{
  template<typename Connection>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  detail::request_type<Connection> req = {}) const -> response
  {
    return request(conn, Method, target, http::empty_body::value_type{}, std::move(req));
  };

  template<typename Connection>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  detail::request_type<Connection> req,
                  system::error_code & ec) const -> response
  {
    return request(conn, Method, target, http::empty_body::value_type{}, std::move(req), ec);
  }

  inline auto operator()(urls::url_view target, http::fields req = {}) const -> response
  {
    return request(Method, target, empty{}, std::move(req));
  }
  inline auto operator()(urls::url_view target, http::fields req, system::error_code & ec) const -> response
  {
    return request(Method, target, empty{}, std::move(req), ec);
  }
};

template<http::verb Method>
struct bind_optional_request : bind_request<Method>, bind_empty_request<Method>
{
  using bind_request<Method>::operator();
  using bind_empty_request<Method>::operator();
};


template<http::verb Method>
struct bind_async_request
{
  template<typename Connection,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
            = typename Connection::default_token>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(Connection & conn,
             urls::url_view target,
             RequestBody && request_body,
             detail::request_type<Connection> req  = {},
             CompletionToken && completion_token = typename Connection::default_token()) const
  {
    return async_request(conn, Method, target,
                         std::forward<RequestBody>(request_body), std::move(req),
                         std::forward<CompletionToken>(completion_token));
  }

  template<typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             CompletionToken && completion_token) const
  {
    return async_request(Method, target,
                         std::forward<RequestBody>(request_body), std::move(req),
                         std::forward<CompletionToken>(completion_token));
  }

};


template<http::verb Method>
struct bind_empty_async_request
{
  template<typename Connection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
            = typename Connection::default_token>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(Connection & conn,
            urls::url_view target,
            detail::request_type<Connection> req = {},
            CompletionToken && completion_token = typename Connection::default_token()) const
  {
    return async_request(conn, Method, target, empty{}, std::move(req),
                         std::forward<CompletionToken>(completion_token));
  }

  template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
  operator()(urls::url_view target, http::fields req, CompletionToken && completion_token) const
  {
    return async_request(Method, target, empty{}, std::move(req),
                         std::forward<CompletionToken>(completion_token));
  }
};

template<http::verb Method>
struct bind_optional_async_request : bind_async_request<Method>, bind_empty_async_request<Method>
{
  using bind_async_request<Method>::operator();
  using bind_empty_async_request<Method>::operator();
};




}
}

#include <boost/requests/impl/request.hpp>

#endif // BOOST_REQUESTS_REQUEST_HPP
