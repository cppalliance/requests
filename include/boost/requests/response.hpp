//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RESPONSE_HPP
#define BOOST_REQUESTS_RESPONSE_HPP

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/basic_dynamic_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/config.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/core/span.hpp>
#include <boost/json/parser.hpp>
#include <boost/json/value.hpp>
#include <boost/requests/error.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <memory>
#include <scoped_allocator>
#include <string>

namespace boost
{
namespace requests
{

template<typename Allocator = std::allocator<char>>
struct basic_response
{
  using allocator_type = Allocator;
  using fields_type = beast::http::basic_fields<allocator_type>;
  using buffer_type = beast::basic_flat_buffer<allocator_type>;
  using body_type = beast::http::basic_dynamic_body<buffer_type>;

  beast::http::response_header<fields_type> header;
  // raw body
  buffer_type buffer{header.get_allocator()};

  basic_response(allocator_type alloc) : header(alloc), buffer(alloc) {}
  basic_response(beast::http::response_header<fields_type> header,
                 buffer_type buffer) : header(std::move(header)), buffer(std::move(buffer)) {}

  basic_response(const basic_response & ) = default;
  basic_response(basic_response && ) noexcept = default;

  basic_response& operator=(const basic_response & ) = default;
  basic_response& operator=(basic_response && ) noexcept = default;

  template<typename Char = char,
           typename CharTraits = std::char_traits<char>>
  auto string_view() const -> basic_string_view<Char, CharTraits>
  {
    const auto cd = buffer.cdata();
    return basic_string_view<Char, CharTraits>(static_cast<const char*>(cd.data()), cd.size());
  }

  template<typename Char = char,
            typename CharTraits = std::char_traits<char>,
            typename Allocator_>
  auto string(Allocator_ && alloc) const -> std::basic_string<Char, CharTraits, std::decay_t<Allocator_>>
  {
    return string_view<Char, CharTraits>().to_string(std::forward<Allocator_>(alloc));
  }


  template<typename Char = char,
           typename CharTraits = std::char_traits<char>>
  auto string() const -> std::basic_string<Char, CharTraits, allocator_type>
  {
    return string_view<Char, CharTraits>().to_string(header.get_allocator());
  }

  template<typename Byte = unsigned char>
  auto raw() const -> span<Byte>
  {
    const auto cd = buffer.cdata();
    return span<Byte>(static_cast<const Byte*>(cd.data()), cd.size() / sizeof(Byte));
  }

  auto json(json::storage_ptr ptr,
            system::error_code & ec) const -> json::value
  {
    json::parser ps;
    ps.write(string_view(), ec);
    if (ec)
      return nullptr;
    else
      return ps.release();
  }

  auto json(json::storage_ptr ptr) const -> json::value
  {
    boost::system::error_code ec;
    auto res = json(ptr, ec);
    if (ec)
      urls::detail::throw_system_error(ec);

    return res;
  }

  auto json(system::error_code & ec) const -> json::value { return json(json::storage_ptr(), ec); }
  auto json()                        const -> json::value { return json(json::storage_ptr()); }


  using string_body_type = typename beast::http::basic_string_body<char, std::char_traits<char>, allocator_type>;
  using history_type = typename beast::http::response<body_type, fields_type>;
  using vector_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<history_type>;

  std::vector<history_type, vector_alloc> history{vector_alloc{header.get_allocator()}};

  bool ok () const
  {
    using namespace beast::http;
    const auto s = to_status_class(header.result());
    return s == status_class::client_error || s == status_class::server_error;
  }
  explicit operator bool() const { return ok(); }

  bool is_redirect() const
  {
    using s = beast::http::status;
    switch (header.result())
    {
    case s::moved_permanently: return true;
    case s::found: return true;
    case s::temporary_redirect: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }
  bool is_permanent_redirect() const
  {
    using s = beast::http::status;
    switch (header.result())
    {
    case s::moved_permanently: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }

  system::error_code status_as_error(boost::source_location loc = BOOST_CURRENT_LOCATION)
  {
    system::error_code res;
    res.assign(header.result_int(), http_status_category(), &loc);
    return res;
  }

  void throw_status_if_error(boost::source_location loc = BOOST_CURRENT_LOCATION)
  {
    auto ec = status_as_error(loc);
    if (ec)
      boost::throw_exception(system::system_error(ec));
  }

  system::result<std::vector<struct link>> link() const
  {
    std::vector<struct link> res;

    for (const auto & l : boost::make_iterator_range(header.equal_range(beast::http::field::link)))
    {
      auto ll = parse_link_field(l.value());
      if (ll.has_error())
        return ll.error();

      res.insert(res.end(), ll->begin(), ll->end());
    }
    return res;
  }
};

using response = basic_response<>;

namespace pmr {
  using response = basic_response<container::pmr::polymorphic_allocator<char>>;
}

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
extern template struct basic_response<std::allocator<char>>;
extern template struct basic_response<container::pmr::polymorphic_allocator<char>>;
#endif

}
}

#endif // BOOST_REQUESTS_RESPONSE_HPP
