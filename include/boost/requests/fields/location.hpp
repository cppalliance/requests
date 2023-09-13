//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FIELDS_LOCATION_HPP
#define BOOST_REQUESTS_FIELDS_LOCATION_HPP

#include <boost/beast/http/message.hpp>
#include <boost/requests/error.hpp>
#include <boost/requests/redirect.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/static_url.hpp>
#include <boost/url/url.hpp>

namespace boost
{
namespace requests
{

BOOST_REQUESTS_DECL system::result<urls::url_view> interpret_location(
    core::string_view current_target,
    core::string_view location);

}
}

#endif // BOOST_REQUESTS_FIELDS_LOCATION_HPP
