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


template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_impl::async_ropen(beast::http::verb method,
                        urls::pct_string_view path,
                        http::fields & headers,
                        source & src,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, stream)>(
      &async_ropen_impl, completion_token,
      this, method, path,
      std::ref(headers), std::ref(src), jar);
}


template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_impl::async_upgrade(urls::pct_string_view path,
                               http::fields & headers,
                               cookie_jar * jar,
                               CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code, websocket)>(
      &async_upgrade_impl, completion_token,
      this, path, std::ref(headers), jar);
}

}

}
}


#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP

