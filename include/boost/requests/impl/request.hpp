//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_REQUEST_HPP
#define BOOST_REQUESTS_IMPL_REQUEST_HPP

#include <boost/requests/http.hpp>
#include <boost/requests/service.hpp>
#include <boost/requests/session.hpp>

namespace boost
{
namespace requests
{


template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             beast::http::verb method,
             urls::url_view target,
             RequestBody && body,
             typename Connection::request_type  req,
             system::error_code & ec) -> response
{
  auto s = conn.ropen(method, target, std::forward<RequestBody>(body), std::move(req), ec);
  response rb{};

  if (!ec)
    s.read( rb.buffer,  ec);
  rb.headers = std::move(s).headers();
  rb.history = std::move(s).history();

  return rb;
}

namespace detail
{

template<typename Connection, typename RequestBody>
struct async_request_op : asio::coroutine
{
  constexpr static const char * op_name = "async_request_op";

  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  Connection & conn;
  http::verb method;
  urls::url_view target;
  RequestBody && request_body;

  struct state_t
  {
    state_t(typename Connection::request_type req) : req(std::move(req)) {}
    typename Connection::request_type req;
    optional<stream> str_;

    response rb{};
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, response)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  template<typename RequestBody_>
  async_request_op(allocator_type alloc,
                   Connection * conn,
                   http::verb method,
                   urls::url_view target,
                   RequestBody_ && request_body,
                   typename Connection::request_type req)
      : conn(*conn), method(method), target(target)
      , request_body(static_cast<RequestBody&&>(request_body))
      , state(allocate_unique<state_t>(alloc, std::move(req))) {}

  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec = {},
                  variant2::variant<std::size_t, stream> s = 0u)
  {
    auto st = state.get();
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD conn.async_ropen(method, target,
                             std::forward<RequestBody>(request_body),
                             std::move(st->req), std::move(self));
      st->str_.emplace(std::move(variant2::get<1>(s)));
      if (!ec)
      {
        BOOST_REQUESTS_YIELD st->str_->async_read( st->rb.buffer, std::move(self));
      }
      st->rb.headers = std::move(*st->str_).headers();
      st->rb.history = std::move(*st->str_).history();
    }
    if (is_complete())
    {
      state.reset();
      self.complete(ec, std::move(st->rb));
    }
  }
};

struct async_free_request_op
{
  template<typename Handler,
           typename RequestBody,
           typename Path>
  void operator()(Handler && handler,
                  beast::http::verb method,
                  Path path,
                  RequestBody && body,
                  http::fields req)
  {
    return async_request(default_session(asio::get_associated_executor(handler)), method, path,
                                         std::forward<RequestBody>(body),
                                         std::move(req), std::move(handler));
  }
};

template<typename Connection, typename RequestBody>
inline void async_request_impl(
    asio::any_completion_handler<void(system::error_code, response)> handler,
    Connection * conn,
    beast::http::verb method,
    urls::url_view target,
    RequestBody * body,
    typename Connection::request_type req)
{
  return asio::async_compose<
      asio::any_completion_handler<void(system::error_code, response)>,
      void(system::error_code, response)>(
        async_request_op<Connection, RequestBody>{
          asio::get_associated_allocator(handler),
          conn, method, target, std::forward<RequestBody>(*body), std::move(req)
      },
      handler, conn->get_executor());
}



}

template<typename Connection,
         typename RequestBody,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, response))
async_request(Connection & conn,
              beast::http::verb method,
              urls::url_view target,
              RequestBody && body,
              typename Connection::request_type req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (system::error_code, response)>(
        &detail::async_request_impl<Connection, RequestBody>,
        completion_token,
        &conn, method, target, &body, std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
async_request(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code,
                                   response)>(
          detail::async_free_request_op{}, completion_token,
          method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
async_request(beast::http::verb method,
              core::string_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void (boost::system::error_code,
                                   response)>(
      detail::async_free_request_op{},
      completion_token, method, path, std::forward<RequestBody>(body), std::move(req));
}


}
}


#endif // BOOST_REQUESTS_REQUEST_HPP
