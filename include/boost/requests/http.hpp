//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_HTTP_HPP
#define BOOST_REQUESTS_HTTP_HPP

#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>

namespace boost {
namespace requests {
namespace http {

using boost::beast::http::field;
using boost::beast::http::status;
using boost::beast::http::status_class;
using boost::beast::http::to_status_class;
using boost::beast::http::to_string;
using boost::beast::http::verb;
using boost::beast::http::fields;

struct header
{
  http::field field = http::field::unknown;
  core::string_view key;
  core::string_view value;
  std::string buffer;

  header() = default;
  header(http::field field, core::string_view value) : field(field), value(value) {}
  header(core::string_view key, core::string_view value) : key(key), value(value) {}
};

struct headers : fields
{
  headers(std::initializer_list<header> fields)
  {
    for (const auto & init : fields)
      if (init.field != http::field::unknown)
        set(init.field, init.value);
      else
        set(init.key, init.value);
  }
  headers(fields && fl) : fields(std::move(fl)) {}
  using fields::fields;
  using fields::operator=;

};

using file_body   = beast::http::file_body;
using empty_body   = beast::http::empty_body;
using string_body = beast::http::string_body;
using buffer_body = beast::http::buffer_body;

using request_header  = beast::http::request_header <boost::beast::http::fields>;
using response_header = beast::http::response_header<boost::beast::http::fields>;

template<typename Body> using request  = beast::http::request <Body, boost::beast::http::fields>;
template<typename Body> using response = beast::http::response<Body, boost::beast::http::fields>;

template<typename Body> using request_parser  = beast::http::request_parser <Body>;
template<typename Body> using response_parser = beast::http::response_parser<Body>;

}

}
}

#endif // BOOST_REQUESTS_HTTP_HPP
