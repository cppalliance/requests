// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/json.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/source.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <thread>
#include <boost/beast/http/read.hpp>

using namespace boost;

BOOST_AUTO_TEST_SUITE(source);

BOOST_AUTO_TEST_CASE(sync)
{
  asio::io_context ctx;
  asio::readable_pipe rp{ctx};
  asio::writable_pipe wp{ctx};
  asio::connect_pipe(rp, wp);

  std::thread thr{
      [&]{
        system::error_code ec;
        auto sp = tag_invoke(requests::make_source_tag{}, json::value{"foobaria"});
        requests::http::fields hd;
        write_request(wp,
                      requests::http::verb::post, "/test", hd,
                      *sp,
                      ec);
        BOOST_CHECK(ec == system::error_code{});
        hd.clear();
        auto ep = requests::make_source(requests::empty());
        write_request(wp,
                      requests::http::verb::get, "/test2", hd,
                      *ep,
                      ec);
        BOOST_CHECK(ec == system::error_code{});
      }};


  beast::http::request<beast::http::string_body> req;
  beast::flat_buffer buf;
  system::error_code ec;
  beast::http::read(rp, buf, req, ec);
  BOOST_CHECK(ec == system::error_code{});
  BOOST_CHECK(req.method() == requests::http::verb::post);
  BOOST_CHECK(req.target() == "/test");
  BOOST_CHECK(req.at(boost::beast::http::field::content_type) == "application/json");
  BOOST_CHECK(json::parse(req.body()) == json::value{"foobaria"});


  beast::http::request<beast::http::empty_body> re2;
  beast::http::read(rp, buf, re2, ec);
  BOOST_CHECK(ec == system::error_code{});
  BOOST_CHECK(re2.method() == requests::http::verb::get);
  BOOST_CHECK(re2.target() == "/test2");
  BOOST_CHECK(re2.count(boost::beast::http::field::content_type) == 0);

  thr.join();
}

BOOST_AUTO_TEST_SUITE_END();