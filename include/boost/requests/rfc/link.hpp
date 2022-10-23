//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_LINK_HPP
#define BOOST_REQUESTS_RFC_LINK_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/system/result.hpp>

namespace boost {
namespace requests {
namespace rfc {

/** A simplified parser for the link-header
 * field specified in  RFC 5988.
 *
 * Due to the complexity, this parser is heavily simplified.
 *
 *
 *
 */

#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ link_value;
#else
struct link_value_t
{
  using value_type = requests::link;

  BOOST_REQUESTS_DECL auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>;

};

constexpr link_value_t link_value{};
#endif


}
}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/rfc/impl/link.ipp>
#endif

#endif // BOOST_REQUESTS_RFC_LINK_HPP
