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
inline bool domain_match(core::string_view full, core::string_view pattern)
{
    if (full.ends_with(pattern))
    {
        if (full.size() == pattern.size())
            return true;
        else
            return full[full.size() - pattern.size() - 1] == '.';
    }
    return false;
}

// the string needs to be normalized to lower-case!
inline bool path_match(core::string_view full, core::string_view pattern)
{
    if (full.starts_with(pattern))
    {
        if (full.size() == pattern.size())
            return true;
        else
            return full[pattern.size()] == '/';
    }
    return false;
}

template<typename Allocator = std::allocator<char>>
struct basic_cookie
{
    using allocator_type = Allocator;
    using string_type = std::basic_string<char, std::char_traits<char>, allocator_type>;

    basic_cookie(allocator_type && alloc) : name(alloc), value(alloc), domain(alloc), path(alloc) {}
    basic_cookie(basic_cookie &&) noexcept = default;

    basic_cookie(basic_cookie && val, allocator_type && alloc)
        : name(std::move(val.name), alloc),
          value(std::move(val.value), alloc),
          expiry_time(val.expiry_time),
          domain(std::move(val.domain), alloc),
          path(std::move(val.path), alloc),
          creation_time(val.creation_time),
          last_access_time(val.last_access_time),
          persistent_flag(val.persistent_flag),
          host_only_flag(val.host_only_flag),
          secure_only_flag(val.secure_only_flag),
          http_only_flag(val.http_only_flag)
    {}

    string_type name, value;
    std::chrono::system_clock::time_point expiry_time;
    string_type domain, path;
    std::chrono::system_clock::time_point creation_time{std::chrono::system_clock::now()},
                                        last_access_time{std::chrono::system_clock::now()};
    bool persistent_flag, host_only_flag, secure_only_flag, http_only_flag;
};

using cookie = basic_cookie<>;

struct cookie_hash
{
    template<typename Allocator>
    std::size_t operator()(const basic_cookie<Allocator> & lhs) const
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
    template<typename Allocator>
    bool operator()(const basic_cookie<Allocator> & lhs, const basic_cookie<Allocator> & rhs) const
    {
        return std::tie(lhs.name, lhs.domain, lhs.path)
            == std::tie(rhs.name, rhs.domain, rhs.path);
    }
};

struct cookie_jar_base
{
  virtual bool set(const set_cookie & set,
                   core::string_view request_host,
                   bool from_non_http_api = false,
                   urls::pct_string_view request_uri_path = "/",
                   const public_suffix_list & public_suffixes = default_public_suffix_list()) = 0;

  virtual auto get(core::string_view request_host,
                   bool is_secure = false,
                   urls::pct_string_view request_uri_path = "/") const
      -> std::string  = 0;

  virtual auto get(core::string_view request_host,
                   boost::container::pmr::polymorphic_allocator<char> alloc,
                   bool is_secure = false,
                   urls::pct_string_view request_uri_path = "/") const
      -> std::basic_string<char, std::char_traits<char>, boost::container::pmr::polymorphic_allocator<char>> = 0;
};

template<typename Allocator = std::allocator<char>>
struct basic_cookie_jar final : cookie_jar_base
{
    using allocator_type = Allocator;
    using cookie_type = basic_cookie<allocator_type>;
    boost::unordered_set<cookie_type, cookie_hash, cookie_equal,
                         typename std::allocator_traits<allocator_type>::template rebind_alloc<cookie>> content;

    basic_cookie_jar(allocator_type allocator = {}) : content(std::move(allocator)) {}

    bool set(const set_cookie & set,
             core::string_view request_host,
             bool from_non_http_api = false,
             urls::pct_string_view request_uri_path = "/",
             const public_suffix_list & public_suffixes = default_public_suffix_list()) override
    {
        // https://www.rfc-editor.org/rfc/rfc6265#section-5.3

        // 2.   Create a new cookie with name cookie-name, value cookie-value.
        cookie_type sc{content.get_allocator()};
        sc.name = set.name;
        sc.value = set.value;

        // 3.   If the cookie-attribute-list contains an attribute with an attribute-name of "Max-Age":
        if (set.max_age != std::chrono::seconds::max())
        {
            sc.expiry_time = (sc.creation_time + set.max_age);
            sc.persistent_flag = false;
        }
        // Otherwise, if the cookie-attribute-list contains an attribute with an attribute-name of "Expires"
        else if (set.expires != std::chrono::system_clock::time_point::max())
        {
            sc.expiry_time = set.expires;
            sc.persistent_flag = true;
        }
        else // Set the cookie's persistent-flag to false.
        {
            sc.expiry_time = std::chrono::system_clock::time_point::max();
            sc.persistent_flag = false;
        }

        // 4. If the cookie-attribute-list contains an attribute with an attribute-name of "Domain":
        if (!set.domain.empty())
        {

            if (is_public_suffix(sc.domain, public_suffixes))
            {
                // ignore invalid cookie unless it's an exact match
                if (request_host != set.domain)
                    return false;
            }
            else if (!domain_match(request_host, set.domain)) // ignore the cookie, trying to set the wrong hostname
                return false;

            sc.domain = set.domain;
            for (auto & c : sc.domain)
                c = urls::grammar::to_lower(c);
            sc.host_only_flag = false;
        }
        else
        {
            sc.host_only_flag = true;
            sc.domain.assign(request_host.begin(), request_host.end());
        }

        if (!set.path.empty())
            sc.path = set.path;
        else
        {
            if (request_uri_path.empty() || request_uri_path.size() == 1u)
                sc.path = "/";
            else
                sc.path = urls::parse_relative_ref(request_uri_path).value().encoded_path();
            for (auto & c : sc.path)
                c = urls::grammar::to_lower(c);
        }
        for (auto & c : sc.path)
            c = urls::grammar::to_lower(c);

        sc.secure_only_flag = set.secure;
        sc.http_only_flag = set.http_only;
        if (from_non_http_api  && sc.http_only_flag)
            return false;

        //    11.  If the cookie store contains a cookie with the same name,
        //        domain, and path as the newly created cookie:
        auto itr = content.find(sc);
        if (itr != content.end())
        {
            if (itr->http_only_flag && from_non_http_api )
                return false;
            sc.creation_time = itr->creation_time;
            content.erase(itr);
        }
        return content.insert(std::move(sc)).second;
    }

    template<typename Allocator_ = std::allocator<char>>
    auto get(core::string_view request_host,
             bool is_secure = false,
             urls::pct_string_view request_uri_path = "/",
             const Allocator_ & alloc = {}) const
        -> std::basic_string<char, std::char_traits<char>, Allocator_>
    {
        auto nw = std::chrono::system_clock::now();
        using alloc_type = typename std::allocator_traits<Allocator_>::template rebind_alloc<char>;
        return make_cookie_field(
                adaptors::filter(
                    content,
                    [&](const cookie_type & ck)
                    {
                        return (is_secure || !ck.secure_only_flag) &&
                               (ck.expiry_time > nw) &&
                               (ck.host_only_flag ? (request_host == ck.domain) : domain_match(request_host, ck.domain)) &&
                               path_match(request_uri_path, ck.path);
                    }), alloc_type(alloc));
    }

    auto get(core::string_view request_host,
             bool is_secure = false,
             urls::pct_string_view request_uri_path = "/") const -> std::string override
    {
      return get<std::allocator<char>>(request_host, is_secure, request_uri_path);
    }

    auto get(core::string_view request_host,
             boost::container::pmr::polymorphic_allocator<char> alloc,
             bool is_secure = false,
             urls::pct_string_view request_uri_path = "/") const
        -> std::basic_string<char, std::char_traits<char>, boost::container::pmr::polymorphic_allocator<char>> override
    {
      return get(request_host, is_secure, request_uri_path, alloc);
    }

    void drop_expired(const std::chrono::system_clock::time_point nw = std::chrono::system_clock::now())
    {
        for (auto itr = content.begin(); itr != content.end(); itr ++)
        {
            if (itr->expiry_time < nw)
                itr = content.erase(itr);
            else
                itr ++;
        }
    }
};

using cookie_jar = basic_cookie_jar<>;

namespace pmr
{
using cookie_jar = basic_cookie_jar<container::pmr::polymorphic_allocator<char>>;
}

template<typename Alloc, typename Allocator>
void tag_invoke(
             const struct prepare_tag &,
             const basic_cookie_jar<Allocator> & jar,
             beast::http::header<true, Alloc> & fields,
             core::string_view request_host,
             bool is_secure,
             system::error_code & )
{
    fields.set(beast::http::field::cookie, jar.get(request_host, is_secure,
                                                   fields.target(), fields.get_allocator()));
}


template<typename Alloc, typename Allocator>
void tag_invoke(
            const struct complete_tag &,
            basic_cookie_jar<Allocator> & jar,
            const beast::http::header<false, Alloc> & fields,
            core::string_view request_host,
            bool is_secure,
            core::string_view target,
            system::error_code & ec)
{
    const auto itr = fields.find(beast::http::field::set_cookie);
    if (itr == fields.end())
        return;
    auto sc = parse_set_cookie_field(itr->value());
    if (sc.has_error())
    {
        ec = sc.error();
        return;
    }
    jar.set(*sc, request_host, false, target);
}

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
extern template struct basic_cookie_jar<std::allocator<char>>;
extern template struct basic_cookie_jar<container::pmr::polymorphic_allocator<char>>;
#endif

}
}

#endif //BOOST_REQUESTS_COOKIES_JAR_HPP
