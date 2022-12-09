// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <boost/json/src.hpp>
#include <boost/url/src.hpp>
#include <boost/requests/src.hpp>

#include "extern.hpp"


namespace boost::requests
{

template struct basic_session<aw_exec>;
template struct basic_connection<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>;
template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>>;
template struct basic_connection_pool<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>;
template struct basic_connection_pool<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>>;

}