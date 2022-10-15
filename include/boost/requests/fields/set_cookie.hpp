// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_SET_COOKIE_HPP
#define BOOST_REQUESTS_COOKIES_SET_COOKIE_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/system/result.hpp>
#include <boost/url/grammar/range_rule.hpp>

namespace boost {
namespace requests {

struct set_cookie
{
    core::string_view name;
    core::string_view value;

    struct extensions_only
    {
        BOOST_URL_DECL bool operator()(const core::string_view & ra) const;
    };

    using attributes_type = urls::grammar::range<urls::string_view>;
    using extensions_type = range_detail::filtered_range<extensions_only, const attributes_type>;


    attributes_type attributes;
    extensions_type extensions() const
    {
        return adaptors::filter(attributes, extensions_only{});
    }


    std::chrono::system_clock::time_point expires{std::chrono::system_clock::time_point::max()};
    std::chrono::seconds max_age{std::chrono::seconds::max()};

    core::string_view domain;
    core::string_view path;
    bool secure = false,
         http_only = false;


};

BOOST_URL_DECL
system::result<set_cookie> parse_set_cookie_field(core::string_view value);


}
}

#endif //BOOST_REQUESTS_COOKIES_SET_COOKIE_HPP
