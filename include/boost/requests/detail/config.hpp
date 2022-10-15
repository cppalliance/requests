//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_CONFIG_HPP
#define BOOST_REQUESTS_DETAIL_CONFIG_HPP

#include <boost/config.hpp>

#ifndef BOOST_REQUESTS_HEADER_ONLY
# ifndef BOOST_REQUESTS_SEPARATE_COMPILATION
#   define BOOST_REQUESTS_HEADER_ONLY 1
# endif
#endif

#if defined(BOOST_REQUESTS_HEADER_ONLY)
# define BOOST_REQUESTS_DECL inline
#else
# define BOOST_REQUESTS_DECL
#endif

#define BOOST_REQUESTS_RETURN_EC(ev)                              \
{                                                                 \
  static constexpr auto loc##__LINE__((BOOST_CURRENT_LOCATION));  \
  return ::boost::system::error_code((ev), &loc##__LINE__);       \
}

#if defined(BOOST_REQUESTS_SOURCE)

#else

#endif

#endif // BOOST_REQUESTS_DETAIL_HPP
