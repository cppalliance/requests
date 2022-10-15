// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_COOKIE_HPP
#define BOOST_REQUESTS_COOKIES_COOKIE_HPP

#include "boost/requests/fields/set_cookie.hpp"
#include <boost/core/detail/string_view.hpp>
#include <string>

namespace boost {
namespace requests {

template<typename Allocator>
struct basic_cookie ;

namespace detail {

inline std::size_t cookie_pair_length(std::pair<core::string_view, core::string_view> p)
{
    return p.first.size() + p.second.size() + 1u;
}

template<typename Allocator>
inline void append_cookie_pair(
        std::basic_string<char, std::char_traits<char>, Allocator> & res,
        std::pair<core::string_view, core::string_view> p)
{
    res += p.first;
    res += '=';
    res += p.second;

}

inline std::size_t cookie_pair_length(const set_cookie &  p)
{
    return p.name.size() + p.value.size() + 1u;
}

template<typename Allocator>
inline void append_cookie_pair(
        std::basic_string<char, std::char_traits<char>, Allocator> & res,
        const set_cookie & p)
{
    res += p.name;
    res += "=";
    res += p.value;
}


template<typename Allocator>
inline std::size_t cookie_pair_length(const basic_cookie<Allocator> &  p)
{
    return p.name.size() + p.value.size() + 1u;
}

template<typename Allocator>
inline void append_cookie_pair(
        std::basic_string<char, std::char_traits<char>, Allocator> & res,
        const basic_cookie<Allocator> & p)
{
    res += p.name;
    res += "=";
    res += p.value;
}


}

template<typename Range,
         typename Allocator = std::allocator<char>>
std::basic_string<char, std::char_traits<char>, Allocator>
        make_cookie_field(Range && range, Allocator && alloc = {})
{
    std::basic_string<char, std::char_traits<char>, Allocator> res{std::forward<Allocator>(alloc)};

    std::size_t sz = 0u;
    for (auto && val : range)
    {
        if (sz != 0u)
            sz += 2u;
        sz += detail::cookie_pair_length(val);
    }
    res.reserve(sz);

    for (auto && val : range)
    {
        if (!res.empty())
            res += "; ";
        detail::append_cookie_pair(res, val);
    }
    return res;
}


}
}

#endif //BOOST_REQUESTS_COOKIES_COOKIE_HPP
