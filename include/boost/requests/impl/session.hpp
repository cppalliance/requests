//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_HPP
#define BOOST_REQUESTS_IMPL_SESSION_HPP

#include <boost/requests/session.hpp>
#include <boost/url/grammar/string_token.hpp>

namespace boost {
namespace requests {



template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
session::async_get_pool(urls::url_view url, CompletionToken && completion_token)
{
  return asio::async_initiate<
          CompletionToken, void (boost::system::error_code, std::shared_ptr<connection_pool>)>(
          &async_get_pool_impl, completion_token, this, url);
}


template<typename RequestBody>
auto session::ropen(
    beast::http::verb method,
    urls::url_view url,
    RequestBody && body,
    http::fields fields,
    system::error_code & ec) -> stream
{
  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);

  if (!is_secure && options_.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  auto src = make_source(body);
  return ropen(method, url, fields, *src, ec);
}


template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, basic_stream<Executor>))
session::async_ropen(beast::http::verb method,
                     urls::url_view path,
                     RequestBody && body,
                     http::fields req,
                     CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, stream)>(
      [this](auto handler,
             beast::http::verb method,
             urls::url_view path,
             RequestBody && body, http::fields req)
      {
        auto source_ptr = requests::make_source(std::forward<RequestBody>(body));
        auto & source = *source_ptr;
        auto alloc = asio::get_associated_allocator(handler, asio::recycling_allocator<void>());
        auto req_ptr = allocate_unique<http::fields>(alloc, std::move(req));
        auto & req_ref = *req_ptr;
        async_ropen(method, path, source, req_ref,
                    asio::consign(std::move(handler), std::move(source_ptr), std::move(req_ptr)));
      },
      completion_token, method, path, std::forward<RequestBody>(body), std::move(req)
  );
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
session::async_ropen(http::verb method,
                     urls::url_view path,
                     source & src,
                     http::fields & headers,
                     CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, stream)>(
      &async_ropen_impl, completion_token,
      this, method, path, &src, &headers);
}

}
}
#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
