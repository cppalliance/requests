// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_PARSER_HPP
#define BOOST_REQUESTS_COOKIES_PARSER_HPP

#include <boost/url/parse.hpp>

namespace boost {
namespace requests {

struct set_cookie
{
    std::string_view value;


    bool http_only = false;
};

}
}

#endif //BOOST_REQUESTS_COOKIES_PARSER_HPP
