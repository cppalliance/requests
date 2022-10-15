//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_REQUESTS_CONNECTION_IPP
#define BOOST_REQUESTS_REQUESTS_CONNECTION_IPP

#if defined(BOOST_REQUESTS_SOURCE)

#include <boost/requests/connection.hpp>

namespace boost {
namespace requests {


template struct basic_connection<asio::ip::tcp::socket>;
template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>;

}
}

#endif

#endif // BOOST_REQUESTS_REQUESTS_CONNECTION_IPP
