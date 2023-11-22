// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_REQUEST_PARAMETERS_HPP
#define BOOST_REQUESTS_REQUEST_PARAMETERS_HPP

#include <boost/beast/core/detail/base64.hpp>
#include <boost/requests/cookie_jar.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/request_options.hpp>
#include <boost/requests/response.hpp>
#include <memory>

namespace boost {
namespace requests {



inline http::header basic_auth(core::string_view username,
                               core::string_view password)
{
  auto sz = beast::detail::base64::encoded_size(username.size() + 1 + password.size());
  std::string res;
  res.resize(sizeof("Basic") + sz);
  const beast::string_view prefix = "Basic ";
  auto itr = std::copy(prefix.begin(), prefix.end(), res.begin());
  const auto data = std::string(username) + ":" + std::string(password);
  beast::detail::base64::encode(&*itr, data.data(), data.size());

  http::header fe;
  fe.field = http::field::authorization;
  fe.value = fe.buffer = std::move(res);
  return fe;
}


inline http::header bearer(core::string_view token)
{
  http::header fe;
  fe.field = http::field::authorization;
  fe.value = fe.buffer = "Bearer " + std::string(token);
  return fe;
}



struct request_parameters
{
  http::headers headers;
  request_options opts{};
  cookie_jar * jar = nullptr;
};

}
}

#endif // BOOST_REQUESTS_REQUEST_PARAMETERS_HPP
