// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_OPTIONS_HPP
#define BOOST_REQUESTS_OPTIONS_HPP

#include <filesystem>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <boost/requests/cookies.hpp>

namespace boost::requests
{

struct options
{
    bool follow_redirects{true};
};

}

#endif //BOOST_REQUESTS_OPTIONS_HPP
