// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_HPP
#define BOOST_REQUESTS_COOKIES_HPP

#include <boost/utility/string_view.hpp>

#include <optional>

namespace boost::requests
{

struct cookie_jar
{
    void set_cookie(string_view name, string_view value,
                    std::optional<std::chrono::steady_clock::time_point> expiry = std::nullopt,
                    bool secure = false,
                    bool http_only = false);
};


}

#endif //BOOST_REQUESTS_COOKIES_HPP
