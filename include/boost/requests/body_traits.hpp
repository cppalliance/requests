// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_BODY_TRAITS_HPP
#define BOOST_REQUESTS_BODY_TRAITS_HPP

#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/core/span.hpp>
#include <boost/beast/http/basic_dynamic_body.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/span_body.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/requests/form.hpp>
#include <boost/url/params_view.hpp>
#include <boost/url/params_encoded_view.hpp>

#include <boost/requests/mime_types.hpp>

#if defined(__cpp_lib_filesystem)
#include <filesystem>
#endif


namespace boost
{
namespace filesystem
{
class path;
}
namespace requests
{

template<typename T, typename = void>
struct request_body_traits;

using empty = typename beast::http::empty_body::value_type;

template<>
struct request_body_traits<empty, void>
{
  constexpr static core::string_view default_content_type( empty )
  {
    return "";
  }

  using body_type = beast::http::empty_body;

  static empty make_body(const empty &, system::error_code & )
  {
    return empty{};
  }
};


template<typename Char, typename Traits, typename Alloc>
struct request_body_traits<std::basic_string<Char, Traits, Alloc>, void>
{
  constexpr static core::string_view default_content_type(const std::basic_string<Char, Traits, Alloc> &)
  {
    return "text/plain; charset=utf-8";
  }
  using body_type = beast::http::basic_string_body<Char, Traits, Alloc>;
  static const typename body_type::value_type & make_body(const std::basic_string<Char, Traits, Alloc> & str,
                                                          system::error_code & ec)
  {
    return str;
  }

  static typename body_type::value_type make_body(std::basic_string<Char, Traits, Alloc> && str,
                                                  system::error_code & ec)
  {
    return std::move(str);
  }
};


template<std::size_t Size>
struct request_body_traits<const char[Size], void>
{
  constexpr static core::string_view default_content_type(const char (&)[Size])
  {
    return "text/plain; charset=utf-8";
  }
  using body_type = beast::http::span_body<char>;

  static typename body_type::value_type make_body(const char (&buf)[Size],
                                                  system::error_code & ec)
  {
    return span<char>{buf, Size};
  }
};


template<typename T, typename Alloc>
struct request_body_traits<std::vector<T, Alloc>, void>
{
  constexpr static core::string_view default_content_type( const std::vector<T, Alloc> &)
  {
    return "application/octet-stream";
  }

  using body_type = beast::http::vector_body<T, Alloc>;
  static typename body_type::value_type make_body(const std::vector<T, Alloc> & str, system::error_code & ec)
  {
    return str;
  }

  static typename body_type::value_type make_body(std::vector<T, Alloc> && str, system::error_code & ec)
  {
    return std::move(str);
  }
};


template<typename Char>
struct request_body_traits<core::basic_string_view<Char>, void>
{
  constexpr static core::string_view default_content_type( core::basic_string_view<Char>)
  {
    return "text/plain; charset=utf-8";
  }

  using body_type = beast::http::span_body<Char>;

  static typename body_type::value_type make_body(std::basic_string_view<Char> str, system::error_code & ec)
  {
    return body_type(str.data(), str.size());
  }
};

template<typename T>
struct request_body_traits<span<T>, void>
{
  constexpr static core::string_view default_content_type( span<T> )
  {
    return "application/octet-stream";
  }

  using body_type = beast::http::span_body<T>;

  static typename body_type::value_type  make_body(span<T> s, system::error_code & ec)
  {
    return body_type(s.data(), s.size());
  }
};

template<>
struct request_body_traits<asio::const_buffer, void>
{
  static core::string_view default_content_type( asio::const_buffer )
  {
    return "application/octet-stream";
  }

  using body_type = beast::http::buffer_body;

  static typename body_type::value_type make_body(const asio::const_buffer & cb, system::error_code & ec)
  {
    return {const_cast<void*>(cb.data()), cb.size(), false};
  }
};

template<>
struct request_body_traits<asio::mutable_buffer, void>
{
  static core::string_view default_content_type( asio::mutable_buffer )
  {
    return "application/octet-stream";
  }

  using body_type = beast::http::buffer_body;

  static typename body_type::value_type make_body(const asio::mutable_buffer & cb, system::error_code & ec)
  {
    return {cb.data(), cb.size(), false};
  }
};

template<>
struct request_body_traits<urls::params_encoded_view, void>
{
  static core::string_view default_content_type( const urls::params_encoded_view &  )
  {
    return "application/x-www-form-urlencoded";
  }
  using body_type = beast::http::string_body;

  static typename body_type::value_type make_body(const urls::params_encoded_view & js, system::error_code & ec)
  {
    using vt = typename body_type::value_type;
    return vt(js.buffer().substr(1));
  }
};



template<>
struct request_body_traits<form, void>
{
  static core::string_view default_content_type( const form &  )
  {
    return "application/x-www-form-urlencoded";
  }
  using body_type = beast::http::string_body;

  static typename body_type::value_type make_body(const form & js, system::error_code & ec)
  {
    using vt = typename body_type::value_type;
    return vt(js.storage.buffer().substr(1));
  }
};


template<>
struct request_body_traits<boost::filesystem::path, void>
{
  template<typename Path>
  static core::string_view default_content_type( const Path & pt)
  {
    const auto & mp = default_mime_type_map();
    const auto ext = pt.extension().string();
    auto itr = mp.find(ext);
    if (itr != mp.end())
      return itr->second;
    else
      return "text/plain";
  }
  using body_type = beast::http::file_body;

  template<typename Path>
  static typename body_type::value_type make_body(const Path & pt, system::error_code & ec)
  {
    typename body_type::value_type file;
    file.open(pt.string().c_str(), beast::file_mode::read, ec);
    return file;
  }
};

#if defined(__cpp_lib_filesystem)
template<>
struct request_body_traits<std::filesystem::path, void>
{
  static core::string_view default_content_type( const std::filesystem::path & pt)
  {
    const auto & mp = default_mime_type_map();
    const auto ext = pt.extension().string();
    auto itr = mp.find(ext);
    if (itr != mp.end())
      return itr->second;
    else
      return "text/plain";
  }
  using body_type = beast::http::file_body;

  static typename body_type::value_type make_body(const std::filesystem::path & pt, system::error_code & ec)
  {
    typename body_type::value_type file;
    file.open(pt.string().c_str(), beast::file_mode::read, ec);
    return file;
  }
};

#endif

}
}

#endif // BOOST_REQUESTS_BODY_TRAITS_HPP
