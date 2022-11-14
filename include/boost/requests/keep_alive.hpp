//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_KEEP_ALIVE_HPP
#define BOOST_REQUESTS_KEEP_ALIVE_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/fields/keep_alive.hpp>
#include <boost/requests/http.hpp>

namespace boost
{
namespace requests
{

BOOST_REQUESTS_DECL bool interpret_keep_alive_response(keep_alive & ka,
                                                       http::fields & res,
                                                       system::error_code & ec);

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/keep_alive.ipp>
#endif

#endif // BOOST_REQUESTS_KEEP_ALIVE_HPP
