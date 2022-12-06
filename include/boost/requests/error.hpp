//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_ERROR_HPP
#define BOOST_REQUESTS_ERROR_HPP

#include <boost/beast/http/status.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace boost {
namespace requests {

/// The type of error code used by the library
using error_code = boost::system::error_code;

/// The type of system error thrown by the library
using system_error = boost::system::system_error;

/// The type of error category used by the library
using error_category = boost::system::error_category;


BOOST_REQUESTS_DECL
error_category & http_status_category();

BOOST_REQUESTS_DECL
error_code make_error(beast::http::status stat);


/// Error codes returned from library operations
enum class error
{
  /// The redirect limit was exceeded
  too_many_redirects = 1,
  /// The redirect is disallowed by the settings
  forbidden_redirect,
  /// The redirect was invalid
  invalid_redirect,
  /// The request violates the tls requirement
  insecure,
  /// The target host is invalid
  wrong_host
};

BOOST_REQUESTS_DECL
error_category & request_category();

BOOST_REQUESTS_DECL
error_code
make_error_code(error e);

} // requests

namespace system {


template<>
struct is_error_code_enum<::boost::requests::error>
{
  static bool const value = true;
};


} // system
} // boost

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/error.ipp>
#endif

#endif // BOOST_REQUESTS_ERROR_HPP
