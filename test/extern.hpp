//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_EXTERN_HPP
#define BOOST_REQUESTS_EXTERN_HPP

#include <boost/requests/connection.hpp>
#include <boost/requests/connection_pool.hpp>
#include <boost/requests/session.hpp>
#include <boost/asio/use_awaitable.hpp>

using aw_exec = boost::asio::use_awaitable_t<>::executor_with_default<boost::asio::any_io_executor>;

namespace boost::requests
{

extern template struct basic_session<aw_exec>;
extern template struct basic_connection<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>>;
extern template struct basic_connection_pool<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>;
extern template struct basic_connection_pool<asio::ssl::stream<asio::ip::tcp::socket::rebind_executor<aw_exec>::other>>;

}

#endif // BOOST_REQUESTS_EXTERN_HPP
