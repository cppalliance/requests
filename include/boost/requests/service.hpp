//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SERVICE_HPP
#define BOOST_REQUESTS_SERVICE_HPP

#include <boost/requests/session.hpp>

namespace boost
{
namespace requests
{


struct session_service : asio::detail::execution_context_service_base<session_service>
{
  using executor_type = asio::any_io_executor;
  using session_type = basic_session<executor_type>;

  session_service(asio::execution_context & ctx)
      : asio::detail::execution_context_service_base<session_service>(ctx)
  {
  }
  void shutdown() override
  {
    if (session)
      session->shutdown();
  }

  void destroy()
  {
    session = boost::none;
  }

  boost::optional<session> session{asio::system_executor()};
};


inline auto default_session(asio::any_io_executor exec = asio::system_executor()) -> session &
{
  auto & so = asio::use_service<session_service>(exec.context()).session;
  if (!so)
    so.emplace(exec);
  return *so;
}


template<typename RequestBody>
inline auto request(beast::http::verb method,
                    core::string_view path,
                    RequestBody && body,
                    http::fields req,
                    system::error_code & ec) -> response
{
  return default_session().request(method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody>
inline auto request(beast::http::verb method,
                    core::string_view path,
                    RequestBody && body,
                    http::fields req) -> response
{
  return default_session().request(method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody>
inline auto request(beast::http::verb method,
                    urls::url_view path,
                    RequestBody && body,
                    http::fields req,
                    system::error_code & ec) -> response
{
  return default_session().request(method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody>
inline auto request(beast::http::verb method,
                    urls::url_view path,
                    RequestBody && body,
                    http::fields req) -> response
{
  return default_session().request(method, path, std::forward<RequestBody>(body), std::move(req));
}


template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (system::error_code, response))
async_request(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(system::error_code, response)>(
      [](auto && handler,
         beast::http::verb method,
         urls::url_view path,
         auto && body,
         http::fields req)
      {
        default_session(asio::get_associated_executor(handler)).
          async_request(method, path, std::forward<RequestBody>(body), std::move(req), std::move(handler));
      },
      completion_token,
      method, path,
      std::forward<RequestBody>(body),
      std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (system::error_code, response))
async_request(beast::http::verb method,
              core::string_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(system::error_code, response)>(
      [](auto && handler,
         beast::http::verb method,
         core::string_view path,
         RequestBody && body,
         http::fields req)
      {
        default_session(asio::get_associated_executor(handler)).
            async_request(method, path, std::forward<RequestBody>(body), std::move(req), std::move(handler));
      },
      completion_token, method, path,
      std::forward<RequestBody>(body),
      std::move(req));
}


}
}

#endif // BOOST_REQUESTS_SERVICE_HPP
