// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/json.hpp>
#include <boost/requests/download.hpp>
#include <boost/requests/form.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request.hpp>

#include "../coroutine_test_case.hpp"
#include "../fixtures/httpbin.hpp"

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
namespace core = boost::core;

using boost::system::error_code;
#if defined(BOOST_REQUESTS_USE_STD_FS)
using fs_error_code = std::error_code;
#else
using fs_error_code = boost::system::error_code;
#endif

#define BOOST_CHECK_HTTP_RESULT(Response) \
  BOOST_CHECK_MESSAGE(boost::requests::http::to_status_class(Response.result()) == requests::http::status_class::successful, Response)

BOOST_FIXTURE_TEST_SUITE(method, httpbin);


BOOST_AUTO_TEST_CASE(request)
{
  requests::default_options().enforce_tls = false;
  requests::default_options().max_redirects = 5;

  auto u = [this](core::string_view path) { urls::url uu = url(); uu.set_encoded_path(path); return uu; };

  // headers
  {
    auto hdr = requests::request(requests::http::verb::get,
                          u("/headers"),
                          requests::empty{},
                          requests::http::headers({{"Test-Header", "it works"}}));
    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK_EQUAL(hd.at("Host"), json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  // get
  {
    auto hdr = requests::get(u("/get"), requests::http::headers({{"Test-Header", "it works"}}));

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }
/*
  // get-redirect
  {
    auto uu = u("/redirect-to");
    uu.set_encoded_query("url=%2Fget");
    auto hdr = requests::get(uu, requests::http::headers({{"Test-Header", "it works"}}));

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    BOOST_CHECK(hdr.history.size() == 1u);
    BOOST_CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }
*/
  // too-many-redirects
  {
    boost::system::error_code ec;
    requests::default_session().options().max_redirects = 3;
    auto res = requests::get(u("/redirect/10"), {}, ec);
    BOOST_CHECK(res.history.size() == 3);
    BOOST_CHECK(res.headers.begin() == res.headers.end());
    BOOST_CHECK(ec == requests::error::too_many_redirects);
  }

  // download
  {
    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    BOOST_CHECK(!filesystem::exists(target));
    auto res = requests::download(u("/image"), {}, target);
    BOOST_CHECK_HTTP_RESULT(res.headers);
    BOOST_CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    BOOST_CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    BOOST_CHECK_MESSAGE(filesystem::exists(target), target);
    fs_error_code ec;
    filesystem::remove(target, ec);
  }

  // download-too-many-redirects
  {
    error_code ec;
    requests::default_options().max_redirects = 3;
    requests::default_session().options().max_redirects = 3;
    const auto target = filesystem::temp_directory_path() / "requests-test.html";
    if (filesystem::exists(target))
      filesystem::remove(target);
    auto res = requests::download(u("/redirect/10"), {}, target, ec);
    BOOST_CHECK(res.history.size() == 3);
    BOOST_CHECK(res.headers.begin() == res.headers.end());

    BOOST_CHECK(ec == requests::error::too_many_redirects);
    BOOST_CHECK(!filesystem::exists(target));
  }

  // delete
  {
    auto hdr = requests::delete_(u("/delete"), json::value{{"test-key", "test-value"}}, {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::patch(u("/patch"), msg, {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // patch-form
  {
    auto hdr = requests::patch(u("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }

  // put-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::put(u("/put"), msg, {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // put-form
  {
    auto hdr = requests::put(u("/put"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }

  // post-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::post(u("/post"), msg, {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // post-form
  {
    auto hdr = requests::post(u("/post"),
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {});
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }
}


BOOST_COROUTINE_TEST_CASE(async_request)
{
  requests::default_options().enforce_tls = false;
  requests::default_options().max_redirects = 5;

  auto u = [this](core::string_view path) { urls::url uu = url(); uu.set_encoded_path(path); return uu; };

  // headers
  {
    auto hdr = requests::async_request(requests::http::verb::get,
                                 u("/headers"),
                                 requests::empty{},
                                 requests::http::headers({{"Test-Header", "it works"}}), yield);

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    BOOST_CHECK_EQUAL(hd.at("Host"), json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  // get
  {
    auto hdr = requests::async_get(u("/get"), requests::http::headers({{"Test-Header", "it works"}}), yield);

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    auto hd = boost::requests::as_json(hdr).at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }
  /*
  // get-redirect
  {
    auto hdr = requests::async_get(u("/redirect-to?url=%2Fget"), requests::http::headers({{"Test-Header", "it works"}}), yield);

    BOOST_CHECK_HTTP_RESULT(hdr.headers);
    BOOST_CHECK(hdr.history.size() == 1u);
    BOOST_CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = boost::requests::as_json(hdr).at("headers");

    BOOST_CHECK(hd.at("Host")        == json::value(url().encoded_host_and_port()));
    BOOST_CHECK(hd.at("Test-Header") == json::value("it works"));
  }
  */
  // too-many-redirects
  {
    boost::system::error_code ec;
    requests::default_session(yield.get_executor()).options().max_redirects = 3;
    auto res = requests::async_get(u("/redirect/10"), {}, yield[ec]);
    BOOST_CHECK_EQUAL(res.history.size(), 3);
    BOOST_CHECK(res.headers.begin() == res.headers.end());
    BOOST_CHECK_MESSAGE(ec == requests::error::too_many_redirects, ec.what());
  }

  // download
  {
    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    BOOST_CHECK(!filesystem::exists(target));
    auto res = requests::async_download(u("/image"), {}, target, yield);
    BOOST_CHECK_HTTP_RESULT(res.headers);
    BOOST_CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    BOOST_CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    BOOST_CHECK_MESSAGE(filesystem::exists(target), target);
    fs_error_code ec;
    filesystem::remove(target, ec);
  }

/*
  // download-redirect
  {

    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    BOOST_CHECK(!filesystem::exists(target));
    auto res = requests::async_download(u("/redirect-to?url=%2Fimage"), {}, target, yield);
    BOOST_CHECK_HTTP_RESULT(res.headers);
    BOOST_CHECK(res.history.size() == 1u);
    BOOST_CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

    BOOST_CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    BOOST_CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    BOOST_CHECK_MESSAGE(filesystem::exists(target), target);
    fs_error_code ec;
    filesystem::remove(target, ec);
  }*/


  // download-too-many-redirects
  {
    error_code ec;
    requests::default_options().max_redirects = 3;
    requests::default_session().options().max_redirects = 3;
    const auto target = filesystem::temp_directory_path() / "requests-test.html";
    if (filesystem::exists(target))
      filesystem::remove(target);
    auto res = requests::async_download(u("/redirect/10"), {}, target, yield[ec]);
    BOOST_CHECK(res.history.size() == 3);
    BOOST_CHECK(requests::is_redirect(res.headers.result()));
    BOOST_CHECK(res.headers.begin() == res.headers.end());

    BOOST_CHECK(ec == requests::error::too_many_redirects);
    BOOST_CHECK(!filesystem::exists(target));
  }

  // delete
  {
    auto hdr = requests::async_delete(u("/delete"), json::value{{"test-key", "test-value"}}, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::async_patch(u("/patch"), msg, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // patch-form
  {
    auto hdr = requests::async_patch(u("/patch"),
                               requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                               {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }

  // put-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::async_put(u("/put"), msg, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // put-form
  {
    auto hdr = requests::async_put(u("/put"),
                             requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                             {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }

  // post-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::async_post(u("/post"), msg, {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/json");
    BOOST_CHECK(js.at("json") == msg);
  }

  // post-form
  {
    auto hdr = requests::async_post(u("/post"),
                              requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                              {}, yield);
    BOOST_CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    BOOST_CHECK(hdr.headers.result() == requests::http::status::ok);
    BOOST_CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    BOOST_CHECK(js.at("form") == (json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}}));
  }
}

BOOST_AUTO_TEST_SUITE_END();