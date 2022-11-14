//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_KEEP_ALIVE_IPP
#define BOOST_REQUESTS_KEEP_ALIVE_IPP

#include <boost/requests/keep_alive.hpp>
#include <chrono>

namespace boost
{
namespace requests
{

bool interpret_keep_alive_response(keep_alive & ka,
                                   http::fields & res,
                                   system::error_code & ec)
{
  bool should_close = false;

  const auto now = std::chrono::system_clock::now();
  const auto conn_itr = res.find(beast::http::field::connection);
  if (ec)
  {
    ka.timeout = std::chrono::system_clock::time_point::min();
    ka.max = 0ull;
    should_close = true;
  }

  if (conn_itr == res.end())
    return should_close;

  if (urls::grammar::ci_is_equal(conn_itr->value(), "close"))
    should_close = true;

  if (!urls::grammar::ci_is_equal(conn_itr->value(), "keep-alive"))
    ec = asio::error::invalid_argument;

  if (!should_close)
  {
    const auto kl_itr = res.find(beast::http::field::keep_alive);
    if (kl_itr == res.end())
      ka = keep_alive{}; // set to max
    else
    {

      auto rr = parse_keep_alive_field(kl_itr->value(), now);
      if (rr.has_error())
        ec = rr.error();
      else
        ka = *rr;

      if (ka.timeout < now)
        should_close = true;
    }
  }

  return should_close;
}

}
}

#endif // BOOST_REQUESTS_KEEP_ALIVE_IPP
