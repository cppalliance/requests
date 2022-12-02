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
#include <boost/container/pmr/polymorphic_allocator.hpp>

namespace boost {
namespace requests {
namespace http {

using boost::beast::http::field;
using boost::beast::http::status;
using boost::beast::http::verb;
using fields = boost::beast::http::basic_fields<boost::container::pmr::polymorphic_allocator<char>>;

using file_body   = beast::http::file_body;
using empty_body   = beast::http::empty_body;
using string_body = beast::http::string_body;
using buffer_body = beast::http::buffer_body;

using request_header  = beast::http::request_header <fields>;
using response_header = beast::http::response_header<fields>;

template<typename Body> using request  = beast::http::request <Body, fields>;
template<typename Body> using response = beast::http::response<Body, fields>;

template<typename Body> using request_parser  = beast::http::request_parser <Body, boost::container::pmr::polymorphic_allocator<char>>;
template<typename Body> using response_parser = beast::http::response_parser<Body, boost::container::pmr::polymorphic_allocator<char>>;

}

}
}

#endif // BOOST_REQUESTS_HTTP_HPP
