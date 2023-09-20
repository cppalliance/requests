//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {



template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
connection_pool::async_lookup(urls::url_view av, CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code)>(
      &async_lookup_impl,
      completion_token,
      this, av);
}



template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
connection_pool::async_get_connection(CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (error_code, connection)>(
      &async_get_connection_impl,
      completion_token,
      this);
}

template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code,
                                   stream))
connection_pool::async_ropen(beast::http::verb method,
                             urls::pct_string_view path,
                             http::fields & headers,
                             source & src,
                             request_options opt,
                             cookie_jar * jar,
                             CompletionToken && completion_token)
{

  return asio::async_initiate<CompletionToken, void (boost::system::error_code, stream)>(
      &async_ropen_impl,
      completion_token,
      this, method, path, std::ref(headers), std::ref(src), std::move(opt), jar);
}

template<typename RequestBody,
          typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_pool::async_ropen(beast::http::verb method,
                             urls::url_view path,
                             RequestBody && body, request_parameters req,
                             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, stream)>(
      [this](auto handler,
             beast::http::verb method,
             urls::url_view path,
             RequestBody && body, request_parameters req)
      {
        auto source_ptr = requests::make_source(std::forward<RequestBody>(body));
        auto & source = *source_ptr;
        auto alloc = asio::get_associated_allocator(handler, asio::recycling_allocator<void>());
        auto header_ptr = allocate_unique<http::fields>(alloc, std::move(req.fields));
        auto & headers = *header_ptr;
        async_ropen(method, path, headers, source, std::move(req.opts), req.jar,
                    asio::consign(std::move(handler), std::move(source_ptr), std::move(header_ptr)));
      },
      completion_token, method, path, std::forward<RequestBody>(body), std::move(req)
  );
}

}
}

#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
