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


template<typename RequestBody>
inline auto request(beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             http::fields req,
             system::error_code & ec) -> response
{
  return default_session().request(method, path, std::forward<RequestBody>(body), std::move(req), ec);
}


namespace detail
{

struct async_free_request_op : asio::coroutine
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
