// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/endpoint.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "doctest.h"
#include "string_maker.hpp"

using namespace boost;

TEST_SUITE_BEGIN("endpoint");

TEST_CASE("ep")
{
  auto loc = asio::local::stream_protocol::endpoint("/home/klemens/dom.sock");
  auto cpt = asio::ip::tcp::endpoint(asio::ip::make_address("1.2.3.4"), 123);

  asio::generic::stream_protocol::endpoint ep1 = loc;
  asio::generic::stream_protocol::endpoint ep2 = cpt;

  CHECK(loc.protocol() == ep1.protocol());
  CHECK(cpt.protocol() == ep2.protocol());
  CHECK(loc.protocol() != ep2.protocol());


}

TEST_SUITE_END();