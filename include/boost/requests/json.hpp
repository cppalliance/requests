//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_JSON_HPP
#define BOOST_REQUESTS_JSON_HPP

#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/json.hpp>
#include <boost/requests/body_traits.hpp>
#include <boost/requests/detail/async_coroutine.hpp>
#include <boost/requests/error.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/system/result.hpp>
#include <boost/json/parser.hpp>
#include <boost/json/value.hpp>

#include <boost/range.hpp>
#include <vector>
#include <boost/asio/yield.hpp>

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>


namespace boost
{
namespace requests
{


template<>
struct request_body_traits<json::value, void>
{
  static core::string_view default_content_type( const json::value &  )
  {
    return "application/json";
  }

  using body_type = beast::http::string_body;

  static typename body_type::value_type make_body(const json::value & js, system::error_code & ec)
  {
    return json::serialize(js);
  }
};

template<>
struct request_body_traits<json::object, void>
{
  static core::string_view default_content_type( const json::object &  )
  {
    return "application/json";
  }

  using body_type = beast::http::string_body;

  static typename body_type::value_type make_body(const json::object & js, system::error_code & ec)
  {
    return json::serialize(js);
  }
};


template<>
struct request_body_traits<json::array, void>
{
  static core::string_view default_content_type( const json::array &  )
  {
    return "application/json";
  }
  using body_type = beast::http::string_body;

  static typename body_type::value_type make_body(const json::array & js, system::error_code & ec)
  {
    return json::serialize(js);
  }
};



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
struct response
{
  using allocator_type = container::pmr::polymorphic_allocator<char>;
  using fields_type = http::fields;

  http::response_header header;

  using value_type = Value;
  value_type value;

  bool ok () const
  {
    using namespace beast::http;
    const auto s = to_status_class(header.result());
    return s == status_class::client_error || s == status_class::server_error;
  }
  explicit operator bool() const { return ok(); }

  bool is_redirect() const
  {
    using s = beast::http::status;
    switch (header.result())
    {
    case s::moved_permanently: return true;
    case s::found: return true;
    case s::temporary_redirect: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }
  bool is_permanent_redirect() const
  {
    using s = beast::http::status;
    switch (header.result())
    {
    case s::moved_permanently: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }

  system::error_code status_as_error(boost::source_location loc = BOOST_CURRENT_LOCATION)
  {
    system::error_code res;
    res.assign(header.result_int(), http_status_category(), &loc);
    return res;
  }

  void throw_status_if_error(boost::source_location loc = BOOST_CURRENT_LOCATION)
  {
    auto ec = status_as_error(loc);
    if (ec)
      boost::throw_exception(system::system_error(ec));
  }

  system::result<std::vector<struct link>> link() const
  {
    std::vector<struct link> res;

    for (const auto & l : boost::make_iterator_range(header.equal_range(beast::http::field::link)))
    {
      auto ll = parse_link_field(l.value());
      if (ll.has_error())
        return ll.error();

      res.insert(res.end(), ll->begin(), ll->end());
    }
    return res;
  }
};

template<typename Value = value, typename Stream>
Value read_json(Stream & str, json::storage_ptr ptr = {})
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[4096];
  while (!sp.done() && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  if (std::is_same<Value, value>::value)
    return sp.release();

  return ::boost::json::value_to<Value>(sp.release());
}

template<typename Value = value,
         typename Stream>
Value read_json(Stream & str,
                json::storage_ptr ptr,
                system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[4096];
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
  char buffer[4096];

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
  char buffer[4096];

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

inline void set_accept_headers(requests::request_settings & hd)
{
  set_accept_headers(hd.fields);
}
template<typename Value = json::value,
         typename Connection>
auto get(Connection & conn,
         typename Connection::target_view target,
         typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  // this might be a bet idea
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req));
  return { s.header(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto get(Connection & conn,
         typename Connection::target_view target,
         typename Connection::request_type req,
         system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req));
  return { s.header(), read_json<Value>(s, ptr, ec) };
}


using requests::head;
using requests::trace;

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto post(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto post(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_json<Value>(s, ec) };
}

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto patch(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_json<Value>(s, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto patch(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
          typename Connection::target_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
         typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ec) };
}



template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::post, target,
                      empty{},
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req));
  return { s.header(), read_optional_json<Value>(s, ec) };
}



template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req));
  return { s.header(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.resource()};
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req));
  return { s.header(), read_json<Value>(s, ptr, ec) };
}

namespace detail
{

template<typename Stream, typename Value>
struct async_read_json_op
{

  using completion_signature_type = void(system::error_code, Value);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream & str;
  ::boost::json::stream_parser sp;
  char buffer[4096];

  async_read_json_op(Stream & str, json::storage_ptr ptr) : str(str), sp{ptr} {}

  Value resume(requests::detail::co_token_t<step_signature_type> self,
               system::error_code & ec, std::size_t n = 0u)
  {
    reenter(this)
    {
      while (!sp.done() && !str.done())
      {
        yield str.async_read_some(asio::buffer(buffer), ec);
        if (ec)
          return Value();
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
    return Value();
  }
};


template<typename Stream, typename Value>
struct async_read_optional_json_op
{

  using completion_signature_type = void(system::error_code, optional<Value>);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream & str;
  ::boost::json::stream_parser sp;
  char buffer[4096];

  async_read_optional_json_op(Stream & str, json::storage_ptr ptr) : str(str), sp{ptr} {}

  optional<Value> resume(requests::detail::co_token_t<step_signature_type> self,
                         system::error_code & ec, std::size_t n = 0u)
  {
    reenter(this)
    {
      yield str.async_read_some(asio::buffer(buffer), ec);
      if (n == 0  && str.done())
        return boost::none;
      sp.write_some(buffer, n, ec);

      while (!sp.done() && !str.done())
      {
        yield str.async_read_some(asio::buffer(buffer), ec);
        if (ec)
          return boost::none;
        sp.write_some(buffer, n, ec);
      }
      if (!ec)
        sp.finish(ec);
      if (ec)
        return boost::none;

      auto res =  ::boost::json::try_value_to<Value>(sp.release());
      if (res.has_error())
      {
        ec = res.error();
        return boost::none;
      }
      else
        return res.value();
    }
    return boost::none;
  }
};

}

template<typename Value = value,
         typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_read_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return requests::detail::co_run<
      detail::async_read_json_op<Stream, Value>>(std::forward<CompletionToken>(completion_token), str, ptr);
}


template<typename Value = value,
          typename Stream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, optional<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::Stream::error_code, optional<Value>))
async_read_optional_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return requests::detail::co_run<
      detail::async_read_optional_json_op<Stream, Value>>(std::forward<CompletionToken>(completion_token), str, ptr);
}

using requests::async_head;
using requests::async_trace;


template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_get(Connection & conn,
          typename Connection::target_view target,
          typename Connection::request_type req = {},
          CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, Value);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, Value>);

    typename Connection::target_view target;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         typename Connection::request_type req) : target(target), req(std::move(req)) {}
    Value resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, Value> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::get, target, empty{}, std::move(req), std::move(self));
        yield async_read_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return Value();
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::move(req));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_post(Connection & conn,
           typename Connection::target_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, Value);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, Value>);

    typename Connection::target_view target;
    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req)
        : target(target), request_body(std::forward<RequestBody>(request_body)), req(std::move(req)) {}
    Value resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, Value> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::post, target,
                               ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                               std::move(req), std::move(self));
        yield async_read_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return Value();
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_patch(Connection & conn,
           typename Connection::target_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, Value);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, Value>);

    typename Connection::target_view target;
    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req)
        : target(target), request_body(std::forward<RequestBody>(request_body)), req(std::move(req)) {}
    Value resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, Value> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::patch, target,
                               ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                               std::move(req), std::move(self));
        yield async_read_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return Value();
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_put(Connection & conn,
            typename Connection::target_view target,
            RequestBody && request_body,
            typename Connection::request_type req = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, optional<Value>);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, optional<Value>>);

    typename Connection::target_view target;
    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req)
        : target(target), request_body(std::forward<RequestBody>(request_body)), req(std::move(req)) {}
    optional<Value> resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, optional<Value>> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::put, target,
                               ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                               std::move(req), std::move(self));
        yield async_read_optional_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return boost::none;
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_delete(Connection & conn,
            typename Connection::target_view target,
            RequestBody && request_body,
            typename Connection::request_type req = {},
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, optional<Value>);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, optional<Value>>);

    typename Connection::target_view target;
    RequestBody && request_body;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         RequestBody && request_body,
         typename Connection::request_type req)
        : target(target), request_body(std::forward<RequestBody>(request_body)), req(std::move(req)) {}
    optional<Value> resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, optional<Value>> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::delete_, target,
                               ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                               std::move(req), std::move(self));
        yield async_read_optional_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return boost::none;
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_delete(Connection & conn,
             typename Connection::target_view target,
             typename Connection::request_type req = {},
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, optional<Value>);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, optional<Value>>);

    typename Connection::target_view target;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         typename Connection::request_type req)
        : target(target), req(std::move(req)) {}
    optional<Value> resume(requests::detail::co_token_t<step_signature_type> self,
                           system::error_code & ec,
                           variant2::variant<typename Connection::stream, optional<Value>> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::delete_, target, empty{}, ptr, std::move(req), std::move(self));
        yield async_read_optional_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return boost::none;
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::move(req));
}


template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_options(Connection & conn,
              typename Connection::target_view target,
              typename Connection::request_type req = {},
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Connection::executor_type))
{
  struct impl : asio::coroutine
  {
    using completion_signature_type = void(system::error_code, Value);
    using step_signature_type       = void(system::error_code, variant2::variant<typename Connection::stream, Value>);

    typename Connection::target_view target;
    typename Connection::request_type req;
    json::storage_ptr ptr{req.get_allocator().resource()};

    impl(typename Connection::target_view target,
         typename Connection::request_type req) : target(target), req(std::move(req)) {}
    Value resume(requests::detail::co_token_t<step_signature_type> self,
                 system::error_code & ec,
                 variant2::variant<typename Connection::stream, Value> s)
    {
      reenter(this)
      {
        yield conn.async_ropen(http::verb::options, target, empty{}, std::move(req), std::move(self));
        yield async_read_json(variant2::get<0>(s), ptr, std::move(self));
        return variant2::get<1>(std::move(s));
      }
      return Value();
    }
  };
  return requests::detail::co_run<impl>(std::forward<CompletionToken>(completion_token),
                                        target, std::move(req));
}

}
}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_JSON_HPP
