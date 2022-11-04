//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_OPTIONS_IPP
#define BOOST_REQUESTS_OPTIONS_IPP

#include <boost/requests/options.hpp>

namespace boost {
namespace requests {

options& default_options()
{
  static options opt;
  return opt;
}

}
}

#endif // BOOST_REQUESTS_OPTIONS_IPP
