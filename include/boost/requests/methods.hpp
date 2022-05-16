// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef REQUESTS_METHODS_HPP
#define REQUESTS_METHODS_HPP

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/requests/optional_ssl.hpp>
#include <boost/requests/options.hpp>
#include <boost/url.hpp>


namespace boost::requests
{

namespace http = boost::beast::http;


}

#endif //REQUESTS_METHODS_HPP
