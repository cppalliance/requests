//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_MIME_TYPES_HPP
#define BOOST_REQUESTS_MIME_TYPES_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/unordered_map.hpp>
#include <boost/url/grammar/ci_string.hpp>

namespace boost {
namespace requests {

using mime_type_map =
    unordered_map<core::string_view,
                  core::string_view,
                  boost::urls::grammar::ci_hash,
                  boost::urls::grammar::ci_equal>;

BOOST_REQUESTS_DECL const mime_type_map & default_mime_type_map();

}
}

#endif // BOOST_REQUESTS_MIME_TYPES_HPP
