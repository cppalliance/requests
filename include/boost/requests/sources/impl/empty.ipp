//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_EMPTY_IPP
#define BOOST_REQUESTS_EMPTY_IPP

#include <boost/requests/sources/empty.hpp>

namespace boost
{
namespace requests
{

empty_source& tag_invoke(const make_source_tag&, const empty &)
{
  static empty_source empty_;
  return empty_;
}

empty_source& tag_invoke(const make_source_tag& tag, const none_t &)
{
  return tag_invoke(tag, empty());
}


}
}

#endif // BOOST_REQUESTS_EMPTY_IPP
