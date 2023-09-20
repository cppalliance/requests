//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_HPP

#include <boost/requests/detail/connection_impl.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/defaulted.hpp>
#include <boost/requests/detail/ssl.hpp>
#include <boost/requests/fields/location.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/stream.hpp>
#include <boost/requests/source.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/core/exchange.hpp>
#include <boost/optional.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/url/grammar/ci_string.hpp>


namespace boost {
namespace requests {
namespace detail {

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
connection_impl::async_connect(endpoint_type ep, CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code)>(
      &async_connect_impl, completion_token,
      this, ep);
}


template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
connection_impl::async_close(CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code)>(
      &async_close_impl, completion_token,
      this);
}

BOOST_REQUESTS_DECL bool check_endpoint(
    urls::url_view path,
    const asio::ip::tcp::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec);

BOOST_REQUESTS_DECL bool check_endpoint(
    urls::url_view path,
    const asio::local::stream_protocol::endpoint & ep,
    core::string_view host,
    bool,
    system::error_code & ec);

BOOST_REQUESTS_DECL bool check_endpoint(
    urls::url_view path,
    const asio::generic::stream_protocol::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec);


template<typename RequestBody>
auto connection_impl::ropen(beast::http::verb method,
           urls::url_view path,
           RequestBody && body, request_parameters req) -> stream
{
  system::error_code ec;
  auto res = ropen(method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}

template<typename RequestBody>
auto connection_impl::ropen(
    beast::http::verb method,
    urls::url_view path,
    RequestBody && body, request_parameters req,
    system::error_code & ec) -> stream
{
  const auto is_secure = use_ssl_;

  if (!detail::check_endpoint(path, endpoint_, host_, use_ssl_, ec))
    return stream{get_executor(), nullptr};

  if (((endpoint_.protocol() == asio::ip::tcp::v4())
    || (endpoint_.protocol() == asio::ip::tcp::v6()))
      && !is_secure && req.opts.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  auto src = requests::make_source(std::forward<RequestBody>(body));
  return ropen(method, path.encoded_target(), req.fields, *src, std::move(req.opts), req.jar, ec);
}

template<typename RequestBody, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code,
                                                         stream))
connection_impl::async_ropen(
    beast::http::verb method,
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

template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_impl::async_ropen(beast::http::verb method,
                        urls::pct_string_view path,
                        http::fields & headers,
                        source & src,
                        request_options opt,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, stream)>(
      &async_ropen_impl, completion_token,
      this, method, path,
      std::ref(headers), std::ref(src), std::move(opt), jar);
}

}

}
}


#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP

