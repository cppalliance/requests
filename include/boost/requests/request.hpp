// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_REQUEST_HPP
#define BOOST_REQUESTS_REQUEST_HPP

#include <boost/requests/cookie_jar.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/options.hpp>
#include <boost/requests/response.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <memory>

namespace boost {
namespace requests {

struct field_entry
{
  variant2::variant<http::field, core::string_view> key;
  core::string_view value;
  std::string buffer;
};

inline field_entry basic_auth(core::string_view username,
                              core::string_view password)
{
  auto sz = beast::detail::base64::encoded_size(username.size() + 1 + password.size());
  std::string res;
  res.resize(sizeof("Basic") + sz);
  constexpr beast::string_view prefix = "Basic ";
  auto itr = std::copy(prefix.begin(), prefix.end(), res.begin());
  const auto data = std::string(username) + ":" + std::string(password);
  beast::detail::base64::encode(&*itr, data.data(), data.size());

  field_entry fe;
  fe.key = http::field::authorization;
  fe.value = fe.buffer = std::move(res);
  return fe;
}


inline field_entry bearer(core::string_view token)
{
  field_entry fe;
  fe.key = http::field::authorization;
  fe.value = fe.buffer = "Bearer " + std::string(token);
  return fe;
}

template<typename Allocator>
inline auto headers(const Allocator & alloc, std::initializer_list<field_entry> fields)
  -> beast::http::basic_fields<Allocator>
{
  beast::http::basic_fields<Allocator> f{alloc};
  for (const auto & init : fields)
    visit(
        [&](auto k)
        {
          f.set(k, init.value);
        }, init.key);
  return f;
}

inline auto headers(std::initializer_list<field_entry> fields) -> beast::http::fields
{
  return headers(std::allocator<char>(), std::move(fields));
}


template<typename Allocator = std::allocator<char>>
struct basic_request
{
  //Allocator
  using allocator_type = Allocator;
  allocator_type get_allocator() const {return allocator;}
  using fields_type = beast::http::basic_fields<allocator_type>;
  allocator_type allocator{};
  fields_type fields{allocator};
  options opts{};
  cookie_jar_base * jar = nullptr;
};

template<>
struct basic_request<std::allocator<char>>
{
  //Allocator
  using allocator_type = std::allocator<char>;
  allocator_type get_allocator() const {return allocator_type();}
  using fields_type = beast::http::basic_fields<allocator_type>;
  fields_type fields{};
  options opts{};
  cookie_jar_base * jar = nullptr;
};


using request = basic_request<>;

namespace pmr {
  using request = basic_request<container::pmr::polymorphic_allocator<char>>;
}

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
extern template struct basic_request<std::allocator<char>>;
extern template struct basic_request<container::pmr::polymorphic_allocator<char>>;
#endif

}
}

#endif //BOOST_REQUESTS_REQUEST_HPP
