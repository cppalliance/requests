// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_JAR_HPP
#define BOOST_REQUESTS_COOKIES_JAR_HPP

#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/requests/fields/set_cookie.hpp>
#include <boost/requests/cookie.hpp>
#include <boost/requests/public_suffix.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/parse_path.hpp>
#include <boost/url/pct_string_view.hpp>
#include <chrono>

#include <boost/beast/http/message.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/url/parse.hpp>

namespace boost {
namespace requests {

// https://www.rfc-editor.org/rfc/rfc6265#section-5.3
// the string needs to be normalized to lower-case!
BOOST_REQUESTS_DECL bool domain_match(core::string_view full, core::string_view pattern);

// the string needs to be normalized to lower-case!
BOOST_REQUESTS_DECL bool path_match(core::string_view full, core::string_view pattern);

struct cookie_hash
{
    std::size_t operator()(const cookie & lhs) const
    {
        size_t seed = 0;
        hash_combine(seed, lhs.name);
        hash_combine(seed, lhs.domain);
        hash_combine(seed, lhs.path);
        return seed;
    }
};

struct cookie_equal
{
    bool operator()(const cookie & lhs, const cookie & rhs) const
    {
        return std::tie(lhs.name, lhs.domain, lhs.path)
            == std::tie(rhs.name, rhs.domain, rhs.path);
    }
};

struct cookie_jar final
{
    using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
    boost::unordered_set<cookie, cookie_hash, cookie_equal,
                         typename std::allocator_traits<allocator_type>::template rebind_alloc<cookie>> content;

    cookie_jar(allocator_type allocator = {}) : content(std::move(allocator)) {}

    BOOST_REQUESTS_DECL bool set(const set_cookie & set,
             core::string_view request_host,
             bool from_non_http_api = false,
             urls::pct_string_view request_uri_path = "/",
             const public_suffix_list & public_suffixes = default_public_suffix_list());

    template<typename StringToken = urls::string_token::return_string>
    auto get(core::string_view request_host,
             bool is_secure = false,
             urls::pct_string_view request_uri_path = "/",
             StringToken && token = {}) const
        -> typename std::decay_t<StringToken>::result_type
    {
        auto nw = std::chrono::system_clock::now();
        return detail::make_cookie_field(
                adaptors::filter(
                    content,
                    [&](const cookie & ck)
                    {
                        return (is_secure || !ck.secure_only_flag) &&
                               (ck.expiry_time > nw) &&
                               (ck.host_only_flag ? (request_host == ck.domain) : domain_match(request_host, ck.domain)) &&
                               path_match(request_uri_path, ck.path);
                    }), static_cast<StringToken&>(token));
    }

    BOOST_REQUESTS_DECL void drop_expired(const std::chrono::system_clock::time_point nw = std::chrono::system_clock::now());
};

}
}

#endif //BOOST_REQUESTS_COOKIES_JAR_HPP
