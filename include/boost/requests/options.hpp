// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_OPTIONS_HPP
#define BOOST_REQUESTS_OPTIONS_HPP

#include <boost/requests/redirect.hpp>

namespace boost {
namespace requests {

/// The basic options attached to any request
struct options
{
  /// Only allow SSL requests
  bool enforce_tls{true};
  /// The allowed redirect mode.
  redirect_mode redirect{private_domain};
  /// The maximum of allowed redirectse
  std::size_t max_redirects{12};

};

}
}
#endif //BOOST_REQUESTS_OPTIONS_HPP
