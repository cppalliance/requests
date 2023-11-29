//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_HTTPBIN_HPP
#define BOOST_REQUESTS_HTTPBIN_HPP

#include <boost/asio/spawn.hpp>
#include <boost/requests/connection.hpp>
#include <boost/url/param.hpp>
#include <boost/url/url.hpp>
#include <string>

struct httpbin
{

  httpbin()
  {
    url_ = boost::urls::url_view("https://httpbin.org");
    if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
      url_ = boost::urls::parse_uri(p).value();

    sslctx.set_default_verify_paths();
  }
  boost::urls::url_view url() const
  {
    return url_;
  }
  boost::requests::connection connect();
  boost::requests::connection async_connect(boost::asio::yield_context yield);


  boost::asio::io_context ctx;
  boost::asio::ssl::context sslctx{boost::asio::ssl::context_base::tlsv12_client};

 private:
   boost::urls::url url_;
};


#endif // BOOST_REQUESTS_HTTPBIN_HPP
