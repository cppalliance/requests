// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_FACADE_HPP
#define BOOST_REQUESTS_FACADE_HPP

#include <boost/beast/http/verb.hpp>
#include <boost/requests/body_traits.hpp>
#include <boost/requests/detail/variadic.hpp>
#include <boost/requests/facade.hpp>
#include <boost/requests/redirect.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/requests/response.hpp>

namespace boost {
namespace requests {

using empty = beast::http::empty_body::value_type;

/// Install all nice methods on the object
template<typename Derived, typename Target, typename Executor>
struct facade
{
  using target_view = Target;
  using executor_type = Executor;

  auto get(target_view target,
           request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::get, target, empty{}, std::move(req));
  }

  auto get(target_view target,
           request_settings req,
           system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::get, target, empty{}, std::move(req), ec);
  }

  auto head(target_view target,
            request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::head, target, empty{}, std::move(req));
  }

  auto head(target_view target,
            request_settings req,
            system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::head, target, empty{}, std::move(req), ec);
  }

  template<typename RequestBody>
  auto post(target_view target,
            RequestBody && request_body,
            request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::post, target,
                                           std::forward<RequestBody>(request_body), std::move(req));
  }

  template<typename RequestBody>
  auto post(target_view target,
            RequestBody && request_body,
            request_settings req,
            system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::post, target,
                                           std::forward<RequestBody>(request_body), std::move(req), ec);
  }


  template<typename RequestBody>
  auto put(target_view target,
           RequestBody && request_body,
           request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::put, target,
                                           std::forward<RequestBody>(request_body), std::move(req));
  }

  template<typename RequestBody>
  auto put(target_view target,
           RequestBody && request_body,
           request_settings req,
           system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::put, target,
                                           std::forward<RequestBody>(request_body), std::move(req), ec);
  }


  template<typename RequestBody>
  auto patch(target_view target,
             RequestBody && request_body,
             request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::patch, target,
                                           std::forward<RequestBody>(request_body), std::move(req));
  }

  template<typename RequestBody>
  auto patch(target_view target,
             RequestBody && request_body,
             request_settings req,
             system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::patch, target,
                                           std::forward<RequestBody>(request_body), std::move(req), ec);
  }

  template<typename RequestBody>
  auto delete_(target_view target,
               RequestBody && request_body,
               request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::delete_, target,
                                           std::forward<RequestBody>(request_body), std::move(req));
  }

  template<typename RequestBody>
  auto delete_(target_view target,
               RequestBody && request_body,
               request_settings req,
               system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::delete_, target,
                                           std::forward<RequestBody>(request_body), std::move(req), ec);
  }


  template<typename RequestBody>
  auto delete_(target_view target,
               request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::delete_, target, empty{}, std::move(req));
  }

  template<typename RequestBody>
  auto delete_(target_view target,
               request_settings req,
               system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::delete_, target, empty{}, std::move(req), ec);
  }


  template<typename RequestBody>
  auto connect(target_view target,
               request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::connect, target, empty{}, std::move(req));
  }

  template<typename RequestBody>
  auto connect(target_view target,
               request_settings req,
               system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::connect, target, empty{}, std::move(req), ec);
  }


  template<typename RequestBody>
  auto options(target_view target,
               request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::options, target, empty{}, std::move(req));
  }

  template<typename RequestBody>
  auto options(target_view target,
               request_settings req,
               system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::options, target, empty{}, std::move(req), ec);
  }


  template<typename RequestBody>
  auto trace(target_view target,
               request_settings req) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::trace, target, empty{}, std::move(req));
  }

  template<typename RequestBody>
  auto trace(target_view target,
             request_settings req,
             system::error_code & ec) -> response
  {
    return static_cast<Derived*>(this)->request(http::verb::trace, target, empty{}, std::move(req), ec);
  }

  template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_get(target_view target,
            request_settings req,
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::get, target,
                                                      empty{}, std::move(req),
                                                      std::forward<CompletionToken>(completion_token));
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_head(target_view target,
             request_settings req,
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::head, target,
                                                       empty{}, std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }

  template<typename RequestBody,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_post(target_view target,
             RequestBody && request_body,
             request_settings req,
             CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::post, target,
                                                      std::forward<RequestBody>(request_body), std::move(req),
                                                      std::forward<CompletionToken>(completion_token));
  }

  template<typename RequestBody,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                  response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_put(target_view target,
            RequestBody && request_body,
            request_settings req,
            CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::put, target,
                                                       std::forward<RequestBody>(request_body), std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }

  template<typename RequestBody,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                  response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_patch(target_view target,
              RequestBody && request_body,
              request_settings req,
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::patch, target,
                                                       std::forward<RequestBody>(request_body), std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }

  template<typename RequestBody,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                  response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_delete(target_view target,
               RequestBody && request_body,
               request_settings req,
               CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::delete_, target,
                                                       std::forward<RequestBody>(request_body), std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }


  template<typename RequestBody,
           BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                  response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_delete(target_view target,
               request_settings req,
               CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::delete_, target,
                                                      empty{}, std::move(req),
                                                      std::forward<CompletionToken>(completion_token));
  }


  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_connect(target_view target,
                request_settings req,
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::connect, target,
                                                       empty{}, std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }


  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_options(target_view target,
                request_settings req,
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::options, target,
                                                       empty{}, std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }


  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                 response)) CompletionToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, response))
  async_trace(target_view target,
              request_settings req,
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return static_cast<Derived*>(this)->async_request(http::verb::trace, target,
                                                       empty{}, std::move(req),
                                                       std::forward<CompletionToken>(completion_token));
  }


};


}
}

#endif //BOOST_REQUESTS_FACADE_HPP
