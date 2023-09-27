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
#include <boost/requests/request_parameters.hpp>
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

inline auto as_json(const response & res,
                    json::storage_ptr ptr,
                    system::error_code & ec) -> json::value
{
  json::parser ps;
  ps.write(res.string_view(), ec);
  if (ec)
    return nullptr;
  else
    return ps.release();
}

inline auto as_json(const response & res, json::storage_ptr ptr = {}) -> json::value
{
  boost::system::error_code ec;
  auto rs = as_json(res, ptr, ec);
  if (ec)
    urls::detail::throw_system_error(ec);

  return rs;
}

inline auto as_json(const response & res, system::error_code & ec) -> json::value
{
  return as_json(res, json::storage_ptr(), ec);
}

namespace json
{

using empty = beast::http::empty_body::value_type;
using ::boost::json::value;
using ::boost::json::storage_ptr;

template<typename Value = value>
struct response : response_base
{
  using allocator_type = std::allocator<char>;
  using fields_type = http::fields;

  response(history_type history, Value && value) : response_base(std::move(history)), value(std::move(value)) {}
  response(http::response_header header, history_type history, Value && value) : response_base(std::move(header), std::move(history)), value(std::move(value)) {}

  response(Value && value = {}) : value(std::move(value)) {}
  response(http::response_header header, Value && value = {}) : response_base(std::move(header)), value(std::move(value)) {}

  using value_type = Value;
  value_type value;

};

template<typename Value = value, typename Stream>
Value read_json(Stream & str, json::storage_ptr ptr = {})
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  return ::boost::json::value_to<Value>(sp.release());
}

template<typename Value = value,
         typename Stream>
Value read_json(Stream & str,
                json::storage_ptr ptr,
                system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !ec && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer), ec);
    if (ec)
      break;
    sp.write_some(buffer, n, ec);
  }
  if (!ec)
    sp.finish(ec);
  if (ec)
    return Value();

  auto res =  ::boost::json::try_value_to<Value>(sp.release());
  if (res.has_error())
  {
    ec = res.error();
    return Value();
  }
  else
    return res.value();
}

template<typename Value = value, typename Stream>
optional<Value> read_optional_json(Stream & str, json::storage_ptr ptr = {})
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  auto n = str.read_some(asio::buffer(buffer));
  if (n == 0u && str.done())
    return boost::none;

  sp.write_some(buffer, n);
  while (!sp.done() && !str.done())
  {
    n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  if (std::is_same<Value, value>::value)
    return sp.release();

  return ::boost::json::value_to<Value>(sp.release());
}

template<typename Value = value,
          typename Stream>
optional<Value> read_optional_json(
                Stream & str,
                json::storage_ptr ptr,
                system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  auto n = str.read_some(asio::buffer(buffer));
  if (n == 0u && str.done())
    return boost::none;
  sp.write_some(buffer, n, ec);

  while (!sp.done() && !ec && !str.done())
  {
    n = str.read_some(asio::buffer(buffer), ec);
    if (ec)
      break;
    sp.write_some(buffer, n, ec);
  }
  if (!ec)
    sp.finish(ec);
  if (ec)
    return Value();

  if (std::is_same<Value, value>::value)
    return sp.release();

  auto res =  ::boost::json::try_value_to<Value>(sp.release());
  if (res.has_error())
  {
    ec = res.error();
    return Value();
  }
  else
    return res.value();
}

inline void set_accept_headers(http::fields & hd)
{
  if (hd.count(http::field::accept) == 0)
    hd.set(http::field::accept, "application/json");
}

inline void set_accept_headers(requests::request_parameters & hd)
{
  set_accept_headers(hd.fields);
}

template<typename Value = json::value,
         typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req = {},
         json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  // this might be a bet idea
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req,
         json::storage_ptr ptr,
         system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req), ec);

  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


using requests::head;
using requests::trace;

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {},
          json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          json::storage_ptr ptr,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {},
          json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {},
          json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         json::storage_ptr ptr,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req = {},
         json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         json::storage_ptr ptr,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {},
             json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req,
             json::storage_ptr ptr,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {},
             json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req,
             json::storage_ptr ptr,
             system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value>
auto get(urls::url_view target,
         http::fields req = {},
         json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  // this might be a bet idea
  auto s = default_session().ropen(http::verb::get, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value>
auto get(urls::url_view target,
         http::fields req,
         json::storage_ptr ptr,
         system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::get, target, empty{}, std::move(req), ec);

  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


using requests::head;
using requests::trace;

template<typename Value = json::value,
          typename RequestBody>
auto post(urls::url_view target,
          RequestBody && request_body,
          http::fields req = {},
          json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto post(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          json::storage_ptr ptr,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

template<typename Value = json::value,
          typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           json::storage_ptr ptr,
           system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req = {},
           json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req = {},
         json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req,
         json::storage_ptr ptr,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req = {},
             json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             json::storage_ptr ptr,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             http::fields req = {},
             json::storage_ptr ptr = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             http::fields req,
             json::storage_ptr ptr,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename Connection>
auto options(urls::url_view target,
             http::fields req = {},
             json::storage_ptr ptr = {}) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::options, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value>
auto options(urls::url_view target,
             http::fields req,
             json::storage_ptr ptr,
             system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  auto s = default_session().ropen(http::verb::options, target, empty{}, std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

namespace detail
{

template<typename Stream>
struct async_read_json_op : asio::coroutine
{
  constexpr static const char * op_name = "async_read_json_op";
  using executor_type = typename Stream::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  Stream &str;

  struct state_t
  {
    state_t(json::storage_ptr ptr) : sp(std::move(ptr)) {}
    ::boost::json::stream_parser sp;
    char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, json::value)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  async_read_json_op(allocator_type alloc, Stream * str, json::storage_ptr ptr)
      : str(*str), state{allocate_unique<state_t>(alloc, std::move(ptr))} {}

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      while (!state->sp.done() && !str.done() && !ec)
      {
        BOOST_REQUESTS_YIELD str.async_read_some(asio::buffer(state->buffer), std::move(self));
        if (ec)
          return self.complete(ec, nullptr);
        state->sp.write_some(state->buffer, n, ec);
      }
      if (!ec)
        state->sp.finish(ec);
      if (ec)
        return self.complete(ec, nullptr);

      auto r = state->sp.release();
      state.reset();
      return self.complete(ec, std::move(r));
    }
  }
};

template<typename Stream>
inline void async_read_json_impl(
    asio::any_completion_handler<void(error_code, json::value)> handler,
    Stream * str, json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, json::value)>,
                             void(error_code, json::value)>(
      async_read_json_op<Stream>{
          asio::get_associated_allocator(handler),
          str, std::move(ptr)},
      handler,
      str->get_executor());
}



template<typename Stream>
struct async_read_optional_json_op : asio::coroutine
{
  constexpr static const char * op_name = "async_read_optional_json_op";

  using executor_type = typename Stream::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  Stream & str;
  ::boost::json::stream_parser sp;
  using allocator_type = asio::any_completion_handler_allocator<char, void(error_code, optional<json::value>)>;
  std::vector<char, allocator_type> buffer;

  async_read_optional_json_op(allocator_type alloc, Stream * str, json::storage_ptr ptr)
      : str(*str), sp{ptr}, buffer(BOOST_REQUESTS_CHUNK_SIZE, std::move(alloc)){}

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));
      if (ec || (n == 0  && str.done()))
        return self.complete(ec, boost::none);
      sp.write_some(buffer.data(), n, ec);
      if (ec)
        return self.complete(ec, boost::none);

      while (!sp.done() && !str.done())
      {
        BOOST_REQUESTS_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));
        if (ec)
          return self.complete(ec, boost::none);
        sp.write_some(buffer.data(), n, ec);
      }
      if (!ec)
        sp.finish(ec);
      if (ec)
        return self.complete(ec, boost::none);
      return self.complete(ec, sp.release());
    }
  }
};


template<typename Stream>
inline void async_read_optional_json_impl(
    asio::any_completion_handler<void(error_code, optional<json::value>)> handler,
    Stream * str, json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, optional<json::value>)>,
                             void(error_code, optional<json::value>)>(
      async_read_optional_json_op<Stream>{
          asio::get_associated_allocator(handler),
          str, std::move(ptr)},
      handler,
      str->get_executor());
}

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
      &detail::async_read_json_impl<Stream>,
      completion_token,
      &str, std::move(ptr));
}


template<typename Stream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, optional<json::value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::Stream::error_code, optional<json::value>))
async_read_optional_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{

  return asio::async_initiate<CompletionToken, void(error_code, optional<json::value>)>(
      &detail::async_read_optional_json_impl<Stream>,
      completion_token,
      &str, std::move(ptr));
}

using requests::async_head;
using requests::async_trace;

namespace detail
{

template<typename Connection, typename Value = json::value, typename RequestBody = empty>
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
            typename Connection::request_type req,
            json::storage_ptr ptr)
        : request_body(std::forward<RequestBody>(request_body)), req(std::move(req)), ptr(std::move(ptr)) {}

    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr;

    optional<stream> str_;
    response<Value> rb;

  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, response<Value>)>;
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
       typename Connection::request_type req,
       json::storage_ptr ptr)
      : conn(*conn), method(method), target(target),
        state(allocate_unique<state_t>(alloc, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr))) {}

  template<typename Self>
  void operator()(Self &&  self,
                  system::error_code ec = {},
                  variant2::variant<variant2::monostate, stream, value> s = variant2::monostate{})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD conn.async_ropen(method, target,
                             value_from(std::forward<RequestBody>(state->request_body),
                                        std::is_same<empty, std::decay_t<RequestBody>>{}),
                             std::move(state->req), std::move(self));
      if (!ec)
      {
        state->str_.emplace(std::move(variant2::get<1>(s)));
        state->rb.headers = std::move(*state->str_).headers();
        state->rb.history = std::move(*state->str_).history();
        BOOST_REQUESTS_YIELD async_read_json(*state->str_, state->ptr, std::move(self));
        if (ec)
          break;
      }
      else
      {
        state->rb.headers = std::move(std::move(variant2::get<1>(s))).headers();
        state->rb.history = std::move(std::move(variant2::get<1>(s))).history();
        break;
      }

      auto v = ::boost::json::try_value_to<Value>(variant2::get<2>(s));
      if (v)
        state->rb.value = std::move(*v);
      else
        ec = v.error();
    }

    if (is_complete())
      return self.complete(ec, std::move(state->rb));
  }
};


template<typename Connection, typename Value = json::value, typename RequestBody = empty>
struct async_request_optional_json_op : asio::coroutine
{
  constexpr static const char * op_name = "async_request_optional_json_op";

  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  Connection & conn;
  http::verb method;
  urls::url_view target;
  struct state_t
  {
    state_t(RequestBody && request_body,
            typename Connection::request_type req,
            json::storage_ptr ptr)
        : request_body(std::forward<RequestBody>(request_body)), req(std::move(req)), ptr(std::move(ptr)) {}

    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr;
    optional<stream> str_;

    response<optional<Value>> rb{};
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, response<optional<Value>>)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;



  template<typename RequestBody_>
  async_request_optional_json_op(allocator_type alloc,
                                 Connection * conn,
                                 http::verb method,
                                 urls::url_view target,
                                 RequestBody_ && request_body,
                                 typename Connection::request_type req,
                                 json::storage_ptr ptr)
      : conn(*conn), method(method), target(target),
        state(allocate_unique<state_t>(alloc, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr))) {}

  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec = {},
                  variant2::variant<variant2::monostate, stream, optional<value>> s = variant2::monostate{})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD conn.async_ropen(method, target,
                             ::boost::json::value_from(std::forward<RequestBody>(state->request_body), state->ptr),
                             std::move(state->req), std::move(self));
      if (!ec)
      {
        state->str_.emplace(std::move(variant2::get<1>(s)));
        state->rb.headers = std::move(*state->str_).headers();
        state->rb.history = std::move(*state->str_).history();
        BOOST_REQUESTS_YIELD async_read_json(*state->str_, state->ptr, std::move(self));
      }
      else
      {
        state->rb.headers = std::move(std::move(variant2::get<1>(s))).headers();
        state->rb.history = std::move(std::move(variant2::get<1>(s))).history();
        break;
      }


      if (variant2::get<2>(s))
      {
        auto v = ::boost::json::try_value_to<Value>(*variant2::get<2>(s));
        if (v)
          state->rb.value = std::move(*v);
        else
          ec = v.error();
      }
    }
    if (is_complete())
      self.complete(ec, std::move(state->rb));
  }
};

template<typename Connection, typename Value, typename RequestBody>
inline void async_request_json_impl(
    asio::any_completion_handler<void(error_code, response<Value>)> handler,
    Connection * conn,
    http::verb method,
    urls::url_view target,
    RequestBody && request_body,
    typename Connection::request_type req,
    json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, response<Value>)>,
                             void(error_code, response<Value>)>(
      async_request_json_op<Connection, Value, RequestBody>{
        asio::get_associated_allocator(handler),
        conn, method, target, std::forward<RequestBody>(request_body), std::move(req),
        std::move(ptr)},
      handler,
      conn->get_executor());
}

template<typename Connection, typename Value, typename RequestBody>
inline void async_request_optional_json_impl(
    asio::any_completion_handler<void(error_code, response<optional<Value>>)> handler,
    Connection * conn,
    http::verb method,
    urls::url_view target,
    RequestBody && request_body,
    typename Connection::request_type req,
    json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, response<optional<Value>>)>,
                             void(error_code, response<optional<Value>>)>(
      async_request_optional_json_op<Connection, Value, RequestBody>{
          asio::get_associated_allocator(handler),
          conn, method, target, std::forward<RequestBody>(request_body), std::move(req),
          std::move(ptr)},
      handler,
      conn->get_executor());
}

}

template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_get(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req = {},
          json::storage_ptr ptr = {},
          CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
          &detail::async_request_json_impl<Connection, Value, empty>, completion_token,
          &conn, http::verb::get, target, empty{}, std::move(req), std::move(ptr));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_post(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           json::storage_ptr ptr = {},
           CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
      &detail::async_request_json_impl<Connection, Value, RequestBody>, completion_token,
                          &conn, http::verb::post,  target, std::forward<RequestBody>(request_body),
                              std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_patch(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           json::storage_ptr ptr = {},
           CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
      &detail::async_request_json_impl<Connection, Value, RequestBody>, completion_token,
      &conn, http::verb::patch,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<optional<Value>>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {},
          json::storage_ptr ptr = {},
          CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<optional<Value>>)>(
      &detail::async_request_optional_json_impl<Connection, Value, RequestBody>, completion_token,
      &conn, http::verb::put,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(Connection & conn,
            urls::url_view target,
            RequestBody && request_body,
            typename Connection::request_type req = {},
            json::storage_ptr ptr = {},
            CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
      &detail::async_request_json_impl<Connection, Value, RequestBody>, completion_token,
      &conn, http::verb::delete_,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {},
             json::storage_ptr ptr = {},
             CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
      &detail::async_request_json_impl<Connection, Value, RequestBody>,
      completion_token, &conn, http::verb::delete_,  target, empty{},
      std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_options(Connection & conn,
              urls::url_view target,
              typename Connection::request_type req = {},
              json::storage_ptr ptr = {},
              CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, response<Value>)>(
      &detail::async_request_json_impl<Connection, Value>, completion_token,
      &conn, http::verb::options,  target, empty{}, std::move(req), std::move(ptr));
}

namespace detail
{

template<typename Value = value>
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
                  typename Connection::request_type req)
  {
    auto & sess = default_session(asio::get_associated_executor(handler));
    detail::async_read_json_impl(
            std::forward<Handler>(handler),
            &sess, method, target,
            std::forward<RequestBody>(request_body), std::move(req));
  }
};

}

template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_get(urls::url_view target,
          http::fields req,
          json::storage_ptr ptr,
          CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, Value)>(
          detail::async_free_request_op<Value>{}, completion_token,
          http::verb::get,  target, empty{}, std::move(req), std::move(ptr));
}

template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_post(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           json::storage_ptr ptr,
           CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, Value)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::post,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_patch(urls::url_view target,
            RequestBody && request_body,
            http::fields req,
            json::storage_ptr ptr,
            CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::patch,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_put(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          json::storage_ptr ptr,
          CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::put,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             json::storage_ptr ptr,
             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::delete_,  target, std::forward<RequestBody>(request_body), std::move(req), std::move(ptr));
}


template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(urls::url_view target,
             http::fields req,
             json::storage_ptr ptr,
             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::delete_,  target, empty{}, std::move(req), std::move(ptr));
}


template<typename Value = value,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_options(urls::url_view target,
              http::fields req,
              json::storage_ptr ptr,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::options,  target, empty{}, std::move(req), std::move(ptr));
}




}
}
}



#endif // BOOST_REQUESTS_JSON_HPP
