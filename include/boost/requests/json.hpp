//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_JSON_HPP
#define BOOST_REQUESTS_JSON_HPP

#include <boost/requests/error.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request.hpp>
#include <boost/requests/sources/json.hpp>
#include <boost/json.hpp>
#include <boost/json/parser.hpp>
#include <boost/json/value.hpp>
#include <boost/system/result.hpp>

#include <boost/range.hpp>
#include <vector>

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>



namespace boost
{
namespace requests
{

BOOST_REQUESTS_DECL
auto as_json(const response & res, json::storage_ptr ptr, system::error_code & ec) -> json::value;
BOOST_REQUESTS_DECL
auto as_json(const response & res, json::storage_ptr ptr = {}) -> json::value;
BOOST_REQUESTS_DECL
auto as_json(const response & res, system::error_code & ec) -> json::value;

namespace json
{

using empty = beast::http::empty_body::value_type;
using ::boost::json::value;
using ::boost::json::storage_ptr;

struct response : response_base
{
  response(history_type history, json::value && value, bool empty_body = false)
      : response_base(std::move(history)), value(std::move(value)), empty_body(empty_body) {}
  response(http::response_header header, history_type history, json::value && value, bool empty_body = false)
      : response_base(std::move(header), std::move(history)), value(std::move(value)), empty_body(empty_body) {}

  response(json::value && value = {}, bool empty_body = false) : value(std::move(value)) {}
  response(http::response_header header, json::value && value = {}, bool empty_body = false)
      : response_base(std::move(header)), value(std::move(value)), empty_body(empty_body) {}

  using value_type = json::value;
  value_type value;
  // if the body was empty and not a literal `null`.
  bool empty_body;
};

BOOST_REQUESTS_DECL
json::value read_json(stream & str, json::storage_ptr ptr = {});
BOOST_REQUESTS_DECL
json::value read_json(stream & str, json::storage_ptr ptr, system::error_code & ec);

inline void set_accept_headers(beast::http::fields & hd)
{
  if (hd.count(http::field::accept) == 0)
    hd.set(http::field::accept, "application/json");
}

inline void set_accept_headers(requests::request_parameters & hd)
{
  set_accept_headers(hd.headers);
}

template<http::verb Method>
struct bind_request
{
  template<typename Connection, typename RequestBody>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  RequestBody && request_body,
                  detail::request_type<Connection> req = {},
                  json::storage_ptr ptr = {}) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(conn, Method, target, std::forward<RequestBody>(request_body), std::move(req));
    return { std::move(s.first).headers(),   std::move(s.second), read_json(s.first, ptr) };
  }

  template<typename Connection, typename RequestBody>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  RequestBody && request_body,
                  detail::request_type<Connection> req,
                  json::storage_ptr ptr,
                  system::error_code & ec) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(conn, Method, target, std::forward<RequestBody>(request_body), std::move(req), ec);
    return { std::move(s.first).headers(),   std::move(s.second), read_json(s.first, ptr, ec) };
  }


  template<typename RequestBody>
  auto operator()(urls::url_view target,
                  RequestBody && request_body,
                  http::headers req = {},
                  json::storage_ptr ptr = {}) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(default_session(), Method, target, std::forward<RequestBody>(request_body), std::move(req));
    return { std::move(s.first).headers(), std::move(s.second), read_json(s.first, ptr) };
  }

  template<typename RequestBody>
  auto operator()(urls::url_view target,
                  RequestBody && request_body,
                  http::headers req,
                  json::storage_ptr ptr,
                  system::error_code & ec) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(default_session(), Method, target, std::forward<RequestBody>(request_body), std::move(req), ec);
    return { std::move(s.first).headers(),   std::move(s.second), read_json(s.first, ptr, ec) };
  }
};


template<http::verb Method>
struct bind_empty_request
{
  template<typename Connection>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  detail::request_type<Connection> req = {},
                  json::storage_ptr ptr = {}) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(conn, Method, target, empty{}, std::move(req));
    auto j = read_json(s.first, ptr);
        return { std::move(s.first).headers(), std::move(s.second), std::move(j)};
  }

  template<typename Connection>
  auto operator()(Connection & conn,
                  urls::url_view target,
                  detail::request_type<Connection> req,
                  json::storage_ptr ptr,
                  system::error_code & ec) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(conn, Method, target, empty{}, std::move(req), ec);
    return { std::move(s.first).headers(),   std::move(s.second), read_json(s.first, ptr, ec) };
  }


  auto operator()(urls::url_view target,
                  http::headers req = {},
                  json::storage_ptr ptr = {}) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(default_session(), Method, target, empty{}, std::move(req));
    return { std::move(s.first).headers(), std::move(s.second), read_json(s.first, ptr) };
  }

  auto operator()(urls::url_view target,
                  http::headers req,
                  json::storage_ptr ptr,
                  system::error_code & ec) const -> response
  {
    set_accept_headers(req);
    auto s = request_stream(default_session(), Method, target, empty{}, std::move(req), ec);
    return { std::move(s.first).headers(), std::move(s.second), read_json(s.first, ptr, ec) };
  }
};

template<http::verb Method>
struct bind_optional_request : bind_request<Method>, bind_empty_request<Method>
{
  using bind_request<Method>::operator();
  using bind_empty_request<Method>::operator();
};


constexpr bind_empty_request<   http::verb::get>     get;
constexpr bind_request<         http::verb::post>    post;
constexpr bind_optional_request<http::verb::patch>   patch;
constexpr bind_request<         http::verb::put>     put;
constexpr bind_optional_request<http::verb::delete_> delete_;


namespace detail
{

BOOST_REQUESTS_DECL
void async_read_json_impl(
    asio::any_completion_handler<void(error_code, json::value)> handler,
    stream * str, json::storage_ptr ptr);

}

template<typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, json::value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, json::value))
async_read_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return asio::async_initiate<CompletionToken, void(error_code, json::value)>(
      &detail::async_read_json_impl,
      completion_token,
      &str, std::move(ptr));
}

using requests::async_head;
using requests::async_trace;

namespace detail
{

template<typename Connection, typename RequestBody = empty>
struct async_request_json_op : asio::coroutine
{
  constexpr static const char * op_name = "async_request_json_op";

  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  Connection & conn;
  http::verb method;
  urls::url_view target;

  struct state_t
  {
    state_t(RequestBody && request_body,
            requests::detail::request_type<Connection> req,
            json::storage_ptr ptr)
        : request_body(std::forward<RequestBody>(request_body)), req(std::move(req)), ptr(std::move(ptr)) {}

    RequestBody && request_body;
    requests::detail::request_type<Connection> req;
    json::storage_ptr ptr;

    optional<stream> str_;
    response rb;

  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, response)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;


  json::value value_from(RequestBody && request_body, std::false_type) const
  {
    return ::boost::json::value_from(std::forward<RequestBody>(request_body), state->ptr);
  }

  empty value_from(RequestBody && request_body, std::true_type) const
  {
    return {};
  }

  async_request_json_op(
       allocator_type alloc,
       Connection * conn,
       http::verb method,
       urls::url_view target,
       RequestBody && request_body,
       requests::detail::request_type<Connection> req,
       json::storage_ptr ptr)
      : conn(*conn), method(method), target(target),
        state(allocate_unique<state_t>(alloc, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr))) {}

  template<typename Self>
  void operator()(Self &&  self,
                  system::error_code ec = {},
                  variant2::variant<variant2::monostate, stream, value> s = variant2::monostate{},
                  history hist = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD async_request_stream(conn, method, target,
                             value_from(std::forward<RequestBody>(state->request_body),
                                        std::is_same<empty, std::decay_t<RequestBody>>{}),
                             std::move(state->req), std::move(self));
      if (!ec)
      {
        state->str_.emplace(std::move(variant2::get<1>(s)));
        state->rb.headers = std::move(*state->str_).headers();
        state->rb.history = std::move(hist);
        BOOST_REQUESTS_YIELD async_read_json(*state->str_, state->ptr, std::move(self));

        if (ec)
          break;
      }
      else
      {
        state->rb.headers = std::move(std::move(variant2::get<1>(s))).headers();
        state->rb.history = std::move(hist);
        break;
      }
      state->rb.value = variant2::get<2>(s);
    }

    if (is_complete())
      return self.complete(ec, std::move(state->rb));
  }
};

template<typename Connection, typename RequestBody>
inline void async_request_json_impl(
    asio::any_completion_handler<void(error_code, response)> handler,
    Connection * conn,
    http::verb method,
    urls::url_view target,
    RequestBody && request_body,
    requests::detail::request_type<Connection> req,
    json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, response)>,
                             void(error_code, response)>(
      async_request_json_op<Connection, RequestBody>{
          asio::get_associated_allocator(handler),
          conn, method, target, std::forward<RequestBody>(request_body), std::move(req),
          std::move(ptr)},
      handler,
      conn->get_executor());
}

struct async_free_request_op
{
  template<typename Handler,
            typename RequestBody,
            typename Connection>
  void operator()(Handler && handler,
                  Connection * conn,
                  http::verb method,
                  urls::url_view target,
                  RequestBody && request_body,
                  requests::detail::request_type<Connection> req)
  {
    auto & sess = default_session(asio::get_associated_executor(handler));
    detail::async_read_json_impl(
        std::forward<Handler>(handler),
        &sess, method, target,
        std::forward<RequestBody>(request_body), std::move(req));
  }
};


}

template<http::verb Method>
struct bind_async_request
{
  template<typename Connection,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken
              = typename Connection::default_token>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(Connection & conn,
             requests::detail::target_view<Connection> target,
             RequestBody && request_body,
             requests::detail::request_type<Connection> req  = {},
             json::storage_ptr ptr = {},
             CompletionToken && completion_token = typename Connection::default_token()) const
  {
    return asio::async_initiate<CompletionToken, void (boost::system::error_code, response)>(
        &detail::async_request_json_impl<Connection, RequestBody>,
        completion_token, &conn, Method,  target, std::forward<RequestBody>(request_body),
        std::move(req), std::move(ptr));
  }

  template<typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(urls::url_view target,
             RequestBody && request_body,
             http::headers req,
             json::storage_ptr ptr,
             CompletionToken && completion_token) const
  {
    return asio::async_initiate<CompletionToken,
                                void(boost::system::error_code, json::value)>(
            detail::async_free_request_op{}, completion_token,
            Method,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
  }

};


template<http::verb Method>
struct bind_empty_async_request
{

  template<typename Connection,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken
             = typename Connection::default_token>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response))
  operator()(Connection & conn,
             requests::detail::target_view<Connection> target,
             requests::detail::request_type<Connection> req  = {},
             json::storage_ptr ptr = {},
             CompletionToken && completion_token = typename Connection::default_token()) const
  {
    return asio::async_initiate<CompletionToken, void (boost::system::error_code, response)>(
        &detail::async_request_json_impl<Connection, empty>,
        completion_token, &conn, Method,  target, empty{},
        std::move(req), std::move(ptr));
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  operator()(urls::url_view target,
             http::headers req,
             json::storage_ptr ptr,
             CompletionToken && completion_token) const
  {
    return asio::async_compose<CompletionToken,
                                void(boost::system::error_code, json::value)>(


            detail::async_free_request_op{}, completion_token,
            Method,  target, empty{}, std::move(req), std::move(ptr));
  }
};

template<http::verb Method>
struct bind_optional_async_request : bind_async_request<Method>, bind_empty_async_request<Method>
{
  using bind_async_request<Method>::operator();
  using bind_empty_async_request<Method>::operator();
};


constexpr bind_empty_async_request<   http::verb::get>     async_get;
constexpr bind_async_request<         http::verb::post>    async_post;
constexpr bind_optional_async_request<http::verb::patch>   async_patch;
constexpr bind_async_request<         http::verb::put>     async_put;
constexpr bind_optional_async_request<http::verb::delete_> async_delete;

}
}
}



#endif // BOOST_REQUESTS_JSON_HPP
