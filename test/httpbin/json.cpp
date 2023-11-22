//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/json.hpp>
#include <boost/requests/connection.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/optional/optional_io.hpp>

#include <boost/test/unit_test.hpp>

#include "../fixtures/httpbin.hpp"
#include "../coroutine_test_case.hpp"

#define BOOST_CHECK_HTTP_RESULT(Response) \
  BOOST_CHECK_MESSAGE(boost::requests::http::to_status_class(Response.result()) == requests::http::status_class::successful, Response)


namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
namespace beast = boost::beast;

using boost::system::error_code;


BOOST_FIXTURE_TEST_SUITE(json_connection, httpbin);

BOOST_AUTO_TEST_CASE(sync_http)
{
  auto hc = connect();

  // headers
  {
    auto hdr = request(hc, requests::http::verb::get, urls::url_view("/headers"),
                       requests::empty{},
                       {requests::http::headers({{"Test-Header", "it works"}}), {false}});

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK_EQUAL(hd.at("Host"), json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // stream
  {
    auto hd_ = requests::http::headers({{"Test-Header", "it works"}});
    auto src = requests::make_source(requests::empty{});
    auto str = hc.ropen(requests::http::verb::get, "/get", hd_, *src, nullptr);
    BOOST_CHECK_HTTP_RESULT(str.headers());
    json::stream_parser sp;

    char buf[32];

    error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = str.read_some(asio::buffer(buf), ec);
      BOOST_CHECK(ec == error_code{});
      sp.write_some(buf, sz, ec);
      BOOST_CHECK(ec == error_code{});
    }

    auto hd = sp.release().at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // get-redirect
  {
    auto hdr = requests::json::get(hc, urls::url_view("/redirect-to?url=%2Fget"),
                                   {requests::http::headers({{"Test-Header", "it works"}}), {false}});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    BOOST_CHECK(hdr.history.size() == 1u);
    BOOST_CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = hdr.value.at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // too-many-redirects
  {
    error_code ec;
    auto res = requests::json::get(hc, "/redirect/10",
                                   {{}, {false, requests::redirect_mode::private_domain, 5}}, {}, ec);
    BOOST_CHECK(res.history.size() == 5);
    BOOST_CHECK(res.headers.begin() == res.headers.end());
    BOOST_CHECK(ec == requests::error::too_many_redirects);
  }

  // delete
  {
    auto hdr = requests::json::delete_(hc,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::patch(hc, urls::url_view("/patch"), msg, {{}, {false}});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::put(hc, urls::url_view("/put"), msg, {{}, {false}});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;

    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // post
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::post(hc, urls::url_view("/post"), msg, {{}, {false}});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

}

BOOST_COROUTINE_TEST_CASE(async_http)
{
  auto hc = async_connect(yield);

  // headers
  {
    auto hdr = request(hc, requests::http::verb::get, urls::url_view("/headers"),
                       requests::empty{},
                       {requests::http::headers({{"Test-Header", "it works"}}), {false}});

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // stream
  {
    auto hd_ = requests::http::headers({{"Test-Header", "it works"}});
    auto src = requests::make_source(requests::empty{});
    auto str = hc.async_ropen(requests::http::verb::get, "/get", hd_, *src, nullptr, yield);
    BOOST_CHECK_HTTP_RESULT(str.headers());
    json::stream_parser sp;

    char buf[32];

    error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = str.read_some(asio::buffer(buf), ec);
      BOOST_CHECK(ec == error_code{});
      sp.write_some(buf, sz, ec);
      BOOST_CHECK(ec == error_code{});
    }

    auto hd = sp.release().at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // get-redirect
  {
    auto hdr = requests::json::async_get(hc, "/redirect-to?url=%2Fget",
                                         {requests::http::headers({{"Test-Header", "it works"}}), {false}}, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    BOOST_CHECK(hdr.history.size() == 1u);
    BOOST_CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = hdr.value.at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // too-many-redirects
  {
    error_code ec;
    auto res = requests::json::async_get(hc, "/redirect/10",
                                   {{}, {false, requests::redirect_mode::private_domain, 5}}, {}, yield[ec]);
    BOOST_CHECK(res.history.size() == 5);
    BOOST_CHECK(res.headers.begin() == res.headers.end());
    BOOST_CHECK(ec == requests::error::too_many_redirects);
  }

  // delete
  {
    auto hdr = requests::json::async_delete(hc,  "/delete",
                                            json::value{{"test-key", "test-value"}}, {{}, {false}}, {},  yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::async_patch(hc, "/patch", msg, {{}, {false}}, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::async_put(hc, "/put", msg, {{}, {false}}, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;

    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // post
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::async_post(hc, "/post", msg, {{}, {false}}, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    BOOST_CHECK(hdr.headers.result() == beast::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

}




BOOST_AUTO_TEST_SUITE_END();