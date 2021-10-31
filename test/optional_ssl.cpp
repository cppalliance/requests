// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/optional_ssl.hpp>
#include <boost/asio/use_future.hpp>

namespace boost::requests
{

template struct basic_optional_ssl<asio::any_io_executor> ;
void compile_test(optional_ssl & tcp)
{
    std::string buf;

    std::future<std::size_t> fs;
    std::future<void> f;
    f = tcp.async_connect(asio::ip::tcp::endpoint{}, asio::use_future);

    fs = tcp.async_read_some(asio::buffer(buf), asio::use_future);
    fs = tcp.async_write_some(asio::buffer(buf), asio::use_future);

    f = tcp.async_shutdown(asio::use_future);
    f = tcp.async_handshake(optional_ssl::handshake_type::client, asio::use_future);

}

}