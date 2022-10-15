// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_PUBLIC_SUFFIX_HPP
#define BOOST_REQUESTS_COOKIES_PUBLIC_SUFFIX_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/unordered_set.hpp>

namespace boost {
namespace requests {


struct public_suffix_list
{
    unordered_set<core::string_view> full_matches;
    unordered_set<core::string_view> whitelist;
    unordered_set<core::string_view> wildcards;
};

BOOST_REQUESTS_DECL const public_suffix_list & default_public_suffix_list();
BOOST_REQUESTS_DECL public_suffix_list load_public_suffix_list(core::string_view map);

BOOST_REQUESTS_DECL bool is_public_suffix(core::string_view value,
                                     const public_suffix_list & pse = default_public_suffix_list());


}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/public_suffix.ipp>
#endif

#endif //BOOST_REQUESTS_COOKIES_PUBLIC_SUFFIX_HPP
