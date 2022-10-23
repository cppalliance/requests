//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_ERROR_IPP
#define BOOST_REQUESTS_IMPL_ERROR_IPP

#include <boost/beast/http/status.hpp>
#include <boost/requests/error.hpp>
#include <boost/core/detail/string_view.hpp>
#include <algorithm>

namespace boost {
namespace requests {

struct http_status_category_t final : system::error_category
{
  std::string message( int ev ) const override
  {
    using namespace beast::http;
    return obsolete_reason(static_cast<status>(ev));
  }
  char const * message( int ev, char * buffer, std::size_t len ) const BOOST_NOEXCEPT  override
  {
    using namespace beast::http;
    const auto sv = obsolete_reason(static_cast<status>(ev));
    auto itr = std::copy_n(sv.begin(), (std::min)(len, sv.size() - 1), buffer);
    *itr = '\0';
    return buffer;
  }

  virtual bool failed( int ev ) const BOOST_NOEXCEPT override
  {
    using namespace beast::http;
    const auto s = to_status_class(static_cast<status>(ev));
    return s == status_class::client_error || s == status_class::server_error;
  }

  const char * name() const BOOST_NOEXCEPT override
  {
    return "http.status";
  }
};

error_category & http_status_category()
{
  static http_status_category_t cat;
  return cat;
}

error_code make_error(beast::http::status stat)
{
  return error_code(static_cast<int>(stat), http_status_category());
}

struct request_category_t final : system::error_category
{
  std::string message( int ev ) const override
  {
    using namespace beast::http;
    return message(static_cast<error>(ev)).to_string();
  }
  char const * message( int ev, char * buffer, std::size_t len ) const BOOST_NOEXCEPT  override
  {
    using namespace beast::http;
    const auto sv = message(static_cast<error>(ev));
    auto itr = std::copy_n(sv.begin(), (std::min)(len, sv.size() - 1), buffer);
    *itr = '\0';
    return buffer;
  }

  string_view message( error ev ) const
  {
    if (ev == error{0})
      return "success";
    switch (ev)
    {
    case error::too_many_redirects: return "too-many-redirects";
    case error::forbidden_redirect: return "redirect-forbidden";
    case error::insecure: return "insecure";
    case error::invalid_redirect: return "invalid-redirect";

    default: return "unknown error";
    }
  }

  const char * name() const BOOST_NOEXCEPT override
  {
    return "request.error";
  }
};

BOOST_REQUESTS_DECL
error_category & request_category()
{
  static request_category_t cat;
  return cat;
}

BOOST_REQUESTS_DECL
error_code
make_error_code(error e)
{
  return error_code(static_cast<int>(e), request_category());
}

} // requests
} // boost


#endif // BOOST_REQUESTS_IMPL_ERROR_IPP
