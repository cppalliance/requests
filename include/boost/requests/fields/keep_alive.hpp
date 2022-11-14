//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FIELDS_KEEP_ALIVE_HPP
#define BOOST_REQUESTS_FIELDS_KEEP_ALIVE_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/system/result.hpp>
#include <boost/url/detail/config.hpp>

namespace boost
{
namespace requests
{

struct keep_alive
{
    std::chrono::system_clock::time_point timeout{std::chrono::system_clock::time_point::max()};
    std::size_t max{std::numeric_limits<std::size_t>::max()};
};

BOOST_REQUESTS_DECL system::result<keep_alive>  parse_keep_alive_field(
        core::string_view value,
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

}
}


#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/fields/impl/keep_alive.ipp>
#endif


#endif //BOOST_REQUESTS_FIELDS_KEEP_ALIVE_HPP
