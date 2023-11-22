//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_CHECK_ENDPOINT_HPP
#define BOOST_REQUESTS_CHECK_ENDPOINT_HPP

#include <boost/requests/error.hpp>
#include <boost/requests/redirect.hpp>

#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/url/url_view.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

inline
bool check_endpoint(
    urls::url_view path,
    const asio::ip::tcp::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec)
{
  if ((path.has_port() && (get_port(path) != ep.port()))
      && (path.has_authority() && (path.encoded_host() != host))
      && (path.has_scheme() && (path.host() != (has_ssl ? "https" : "http"))))
    BOOST_REQUESTS_ASSIGN_EC(ec, error::wrong_host)

  return !ec;
}

inline
bool check_endpoint(
    urls::url_view path,
    const asio::local::stream_protocol::endpoint & ep,
    core::string_view host,
    bool,
    system::error_code & ec)
{
  if (path.has_port()
      && (path.has_authority() && (path.host() != host))
      && (path.has_scheme() && (path.host() != "unix")))
    BOOST_REQUESTS_ASSIGN_EC(ec, error::wrong_host)

  return !ec;
}

inline
bool check_endpoint(
    urls::url_view path,
    const asio::generic::stream_protocol::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec)
{
  if (ep.protocol() == asio::local::stream_protocol())
  {
    asio::local::stream_protocol::endpoint cmp;
    cmp.resize(ep.size());
    std::memcpy(cmp.data(),
                ep.data(),
                ep.size());
    return check_endpoint(path, cmp, host, has_ssl, ec);
  }
  else if (ep.protocol() == asio::ip::tcp::v4()
           || ep.protocol() == asio::ip::tcp::v6())
  {
    asio::ip::tcp::endpoint cmp;
    cmp.resize(ep.size());
    std::memcpy(cmp.data(),
                ep.data(),
                ep.size());
    return check_endpoint(path, cmp, host, has_ssl, ec);
  }
  else
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::no_protocol_option);
    return false;
  }
}

}
}
}


#endif // BOOST_REQUESTS_CHECK_ENDPOINT_HPP
