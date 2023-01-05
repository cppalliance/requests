// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCE_HPP
#define BOOST_REQUESTS_SOURCE_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/form.hpp>
#include <boost/requests/http.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/url/params_encoded_view.hpp>

#if defined(__cpp_lib_filesystem)
#include <filesystem>

#endif

namespace boost
{
namespace filesystem
{
class path;
}

namespace json
{

class value;
class object;
class array;

}

namespace requests
{

struct source
{
  virtual ~source() = default;
  virtual optional<std::size_t> size() const = 0;
  virtual void reset() = 0;
  virtual std::pair<std::size_t, bool> read_some(asio::mutable_buffer buffer, system::error_code & ) = 0;
  virtual core::string_view default_content_type() {return "";}
};

using source_ptr = std::shared_ptr<source>;

struct make_source_tag
{
  using allocator_type = container::pmr::polymorphic_allocator<void>;
  allocator_type allocator{container::pmr::get_default_resource()};
  allocator_type get_allocator() const {return allocator;}
};

BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, source_ptr);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, source &);


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


template<typename Char>
struct basic_string_view_source final : source
{
  core::basic_string_view<Char> data;
  std::size_t pos{0};

  basic_string_view_source(std::basic_string_view<Char> data) : data(data) {}

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
    auto res = std::char_traits<Char>::copy(dst, data.c_str() + pos, (std::min)(left, sz));
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
source_ptr tag_invoke(make_source_tag, std::basic_string<Char, Traits, Allocator> data)
{
  return std::make_shared<basic_string_source<Char, Traits, Allocator>>(std::move(data));
}

template<std::size_t N>
source_ptr tag_invoke(make_source_tag, const char (&data)[N])
{
  return std::make_shared<basic_string_view_source<char>>(std::move(data));
}

template<typename View>
auto tag_invoke(make_source_tag, const View & data)
  -> std::enable_if_t<std::is_same<View, core::string_view>::value, source_ptr>
{
  return std::make_shared<basic_string_view_source<typename View::value_type>>(std::move(data));
}

BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, asio::const_buffer cb);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, urls::params_encoded_view);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, form f);

BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, const boost::filesystem::path & path);
#if defined(__cpp_lib_filesystem)
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, const std::filesystem::path & path);
#endif

BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, boost::json::value f);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, boost::json::array f);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, boost::json::object f);

template<typename Stream>
std::size_t write_request(
    Stream & stream,
    http::request_header hd,
    source_ptr src,
    system::error_code & ec);


template<typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t)) CompletionToken
          BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(system::error_code, std::size_t))
   async_write_request(
      Stream & stream,
      http::request_header hd,
      source_ptr src,
      CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(typename Stream::executor_type));


}
}

#include <boost/requests/impl/source.hpp>

#endif //BOOST_REQUESTS_SOURCE_HPP
