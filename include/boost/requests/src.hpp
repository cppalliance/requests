// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SRC_IPP
#define BOOST_REQUESTS_SRC_IPP

#include <boost/requests/detail/config.hpp>

#if defined(BOOST_BEAST_HEADER_ONLY)
#error "You can't include this in header only mode"
#endif

#ifndef BOOST_REQUESTS_SOURCE
#define BOOST_REQUESTS_SOURCE
#endif

#include <boost/requests/fields/impl/keep_alive.ipp>
#include <boost/requests/fields/impl/set_cookie.ipp>
#include <boost/requests/impl/public_suffix.ipp>
#include <boost/requests/impl/redirect.ipp>
#include <boost/requests/rfc/impl/dates.ipp>

#endif //BOOST_REQUESTS_SRC_IPP
