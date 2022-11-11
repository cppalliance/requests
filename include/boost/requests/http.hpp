//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_HTTP_HPP
#define BOOST_REQUESTS_HTTP_HPP

#include <boost/beast/http/fields.hpp>
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
}

}
}

#endif // BOOST_REQUESTS_HTTP_HPP
