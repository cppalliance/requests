// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_COOKIE_HPP
#define BOOST_REQUESTS_COOKIES_COOKIE_HPP

#include <boost/requests/fields/set_cookie.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/url/grammar/string_token.hpp>
#include <boost/core/detail/string_view.hpp>
#include <string>

namespace boost {
namespace requests {

struct cookie
{
  using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
  using string_type = std::basic_string<char, std::char_traits<char>, allocator_type>;

  cookie(allocator_type && alloc) : name(alloc), value(alloc), domain(alloc), path(alloc) {}
  cookie(cookie &&) noexcept = default;

  cookie(cookie && val, allocator_type && alloc)
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


namespace detail {

inline std::size_t cookie_pair_length(std::pair<core::string_view, core::string_view> p)
{
    return p.first.size() + p.second.size() + 1u;
}

inline char* append_cookie_pair(
        char * res,
        std::pair<core::string_view, core::string_view> p)
{
    res = std::copy(p.first.begin(), p.first.end(), res);
    *(res++) = '=';
    res= std::copy(p.second.begin(), p.second.end(), res);
    return res;
}

inline std::size_t cookie_pair_length(const set_cookie &  p)
{
    return p.name.size() + p.value.size() + 1u;
}

inline char * append_cookie_pair(
        char * res,
        const set_cookie & p)
{
    res = std::copy(p.name.begin(), p.name.end(), res);
    *(res++) = '=';
    res= std::copy(p.value.begin(), p.value.end(), res);
    return res;
}


inline std::size_t cookie_pair_length(const cookie &  p)
{
    return p.name.size() + p.value.size() + 1u;
}

inline char * append_cookie_pair(
        char * res,
        const cookie & p)
{
    res = std::copy(p.name.begin(), p.name.end(), res);
    *(res++) = '=';
    res= std::copy(p.value.begin(), p.value.end(), res);
    return res;
}


template<typename Range,
         typename StringToken = urls::string_token::return_string>
auto make_cookie_field(Range && range, StringToken && token = {})
    -> typename std::decay_t<StringToken>::result_type
{
    std::size_t sz = 0u;
    for (auto && val : range)
    {
        if (sz != 0u)
            sz += 2u;
        sz += detail::cookie_pair_length(val);
    }
    auto res = token.prepare(sz);
    bool initial = true;
    for (auto && val : range)
    {
        if (!initial)
        {
          *(res++) = ';';
          *(res++) = ' ';
        }
        initial = false;
        res = detail::append_cookie_pair(res, val);
    }
    return token.result();
}

struct monotonic_token : urls::grammar::string_token::arg
{
  unsigned char buf[4096];
  boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};

  char * data = nullptr;
  std::size_t size = 0u;

  monotonic_token() = default;
  monotonic_token(const monotonic_token &) = delete;

  using result_type = core::string_view;

  char * prepare(std::size_t size_)
  {
    if (size_ == 0u)
      return nullptr;
    else
      return data = static_cast<char*>(memres.allocate(size = size_, 1u));
  }

  core::string_view result() const
  {
    return core::string_view(data, size);
  }

};

}

}
}

#endif //BOOST_REQUESTS_COOKIES_COOKIE_HPP
