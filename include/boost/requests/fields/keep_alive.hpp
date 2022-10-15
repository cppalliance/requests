//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_KEEP_ALIVE_HPP
#define BOOST_REQUESTS_KEEP_ALIVE_HPP

#include <boost/beast/http.hpp>
#include <boost/url/detail/config.hpp>
#include <boost/system/result.hpp>

namespace boost
{
namespace requests
{

struct keep_alive
{
    std::chrono::system_clock::time_point timeout{std::chrono::system_clock::time_point::max()};
    std::size_t max{std::numeric_limits<std::size_t>::max()};
};

BOOST_URL_DECL system::result<keep_alive>  parse_keep_alive_field(
        core::string_view value,
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

}
}

#endif //BOOST_REQUESTS_KEEP_ALIVE_HPP
