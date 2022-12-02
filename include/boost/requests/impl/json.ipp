//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_JSON_IPP
#define BOOST_REQUESTS_IMPL_JSON_IPP

#include <boost/requests/json.hpp>
#include <boost/asio/yield.hpp>

namespace boost
{
namespace requests
{
namespace json
{
namespace detail
{

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
template struct async_read_json_op<stream>;
template struct async_read_optional_json_op<stream>;
#endif

}
}
}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_JSON_IPP
