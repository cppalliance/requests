//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_IMPL_SSL_IPP
#define BOOST_REQUESTS_DETAIL_IMPL_SSL_IPP

#include <boost/requests/detail/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ip/address.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

bool do_verify_host(SSL * ssl, const std::string & host)
{
  X509 * cert = SSL_get_peer_certificate(ssl);
  if (cert == nullptr)
    boost::throw_exception(std::invalid_argument("no peer certificate found"));
  boost::system::error_code ec;
  asio::ip::address address = asio::ip::make_address(host, ec);
  if (!ec)
    return X509_check_ip_asc(cert, host.c_str(), 0) == 1;
  else
  {
    char* peername = nullptr;
    const int result = X509_check_host(cert, host.c_str(), host.size(), 0, &peername);
    OPENSSL_free(peername);
    return result == 1;
  }
}

}
}
}

#endif // BOOST_REQUESTS_DETAIL_IMPL_SSL_IPP
