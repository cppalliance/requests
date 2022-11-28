//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/requests/detail/ssl.hpp>

#include "doctest.h"

using namespace boost;

TEST_CASE("ssl")
{
  asio::io_context ctx;

  asio::ip::tcp::resolver res{ctx};

  char host[] = "httpbin.org";
  auto ep = res.resolve(host, "https");

  REQUIRE(ep.size() > 0);

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};
  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  asio::ssl::stream<asio::ip::tcp::socket > s{ctx, sslctx};

  auto hh = asio::ssl::host_name_verification(host);
  s.set_verify_callback(asio::ssl::host_name_verification(host));

  s.next_layer().connect(*ep);
  s.handshake(asio::ssl::stream_base::client);

  CHECK(requests::detail::verify_host(s, "httpbin.org"));
  CHECK(!requests::detail::verify_host(s, "boost.org"));
  CHECK(requests::detail::verify_host(s, "www.httpbin.org"));
  CHECK(requests::detail::verify_host(s, "api.httpbin.org"));
  CHECK(!requests::detail::verify_host(s, "too.many.subdomains.httpbin.org"));

}

