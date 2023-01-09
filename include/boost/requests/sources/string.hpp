//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCES_STRING_HPP
#define BOOST_REQUESTS_SOURCES_STRING_HPP

#include <boost/requests/source.hpp>
#include <string>
#include <utility>

namespace boost
{
namespace requests
{
template<typename Char, typename Traits, typename Allocator>
struct basic_string_source final : source
{
  std::basic_string<Char, Traits, Allocator> data;
  std::size_t pos{0};

  basic_string_source(std::basic_string<Char, Traits, Allocator> data) : data(std::move(data)) {}

  optional<std::size_t> size() const final
  {
    return data.size();
  }

  void reset() final
  {
    pos = 0;
  }

  std::pair<std::size_t, bool> read_some(asio::mutable_buffer buffer, system::error_code & ) final
  {
    const auto left = data.size() - pos;
    const auto sz = buffer.size() / sizeof(Char);
    auto dst = static_cast<char*>(buffer.data());
    auto res = Traits::copy(dst, data.c_str() + pos, (std::min)(left, sz));
    std::size_t n = std::distance(dst, res);
    pos += n;
    return {n, pos != data.size()};
  }

  core::string_view default_content_type() final
  {
    BOOST_IF_CONSTEXPR (sizeof(Char) == 1)
    return "text/plain; charset=utf-8";
    else BOOST_IF_CONSTEXPR (sizeof(Char) == 2)
        return "text/plain; charset=utf-16";
    else BOOST_IF_CONSTEXPR (sizeof(Char) == 4)
        return "text/plain; charset=utf-32";
    return "";
  }
};

template<typename Char, typename Traits, typename Allocator>
inline basic_string_source<Char, Traits, Allocator> tag_invoke(make_source_tag, std::basic_string<Char, Traits, Allocator> data)
{
  return basic_string_source<Char, Traits, Allocator>(std::move(data));
}

}
}

#endif // BOOST_REQUESTS_SOURCES_STRING_HPP
