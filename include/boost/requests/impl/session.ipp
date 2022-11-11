//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_IPP
#define BOOST_REQUESTS_IMPL_SESSION_IPP


#if defined(BOOST_REQUESTS_SOURCE)

#include <boost/requests/session.hpp>

namespace boost {
namespace requests {

template struct basic_session<asio::any_io_executor>;
template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::empty_body>;
template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::string_body>;

}
}

#endif

#endif // BOOST_REQUESTS_IMPL_SESSION_IPP
