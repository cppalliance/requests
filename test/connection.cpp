// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/download.hpp>
#include <boost/requests/connection.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/form.hpp>
#include <boost/json.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

#include "doctest.h"
#include "string_maker.hpp"

#include <iostream>

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;

#if defined(BOOST_REQUESTS_USE_STD_FS)
using boost::system::error_code;
#else
using std::error_code;
#endif



inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}




TEST_SUITE_BEGIN("connection");

void http_request_connection(bool https)
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  auto hc = https ? requests::connection(ctx.get_executor(), sslctx) : requests::connection(ctx.get_executor());
  hc.set_host(url);
  asio::ip::tcp::resolver rslvr{ctx};
  asio::ip::tcp::endpoint ep = *rslvr.resolve(url, https ? "https" : "http").begin();

  hc.connect(ep);

  // SUBCASE("headers")
  {
    auto hdr = request(hc, requests::http::verb::get, urls::url_view("/headers"),
                          requests::empty{},
                          {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // SUBCASE("stream")
  {
    auto str = hc.ropen(requests::http::verb::get, urls::url_view("/get"), 
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});

    json::stream_parser sp;

    char buf[32];

    error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = str.read_some(asio::buffer(buf), ec);
      CHECK(ec == error_code{});
      sp.write_some(buf, sz, ec);
      CHECK(ec == error_code{});
    }

    auto hd = sp.release().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  // SUBCASE("stream-all")
  {
    auto str = hc.ropen(requests::http::verb::get, urls::url_view("/get"),
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});

    std::string buf;
    auto bb = asio::dynamic_buffer(buf);

    error_code ec;
    CHECK(str.read(bb, ec) > 0u);

    CHECK(ec == error_code{});

    auto val = json::parse(buf);

    auto hd = val.at("headers");
    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  // SUBCASE("stream-dump")
  {
    auto str = hc.ropen(requests::http::verb::get, urls::url_view("/get"), 
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});
    str.dump();
  }

  // SUBCASE("get")
  {
    auto hdr = get(hc, urls::url_view("/get"), {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // SUBCASE("get-redirect")
  {
    auto hdr = get(hc, urls::url_view("/redirect-to?url=%2Fget"),
                   {requests::headers({{"Test-Header", "it works"}}), {false}});

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // SUBCASE("too-many-redirects")
  {
    error_code ec;
    auto res = get(hc, urls::url_view("/redirect/10"), {{}, {false, requests::redirect_mode::private_domain, 5}}, ec);
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  // SUBCASE("download")
  {
    error_code ec;
    const auto tmp = filesystem::temp_directory_path(ec);
    check_ec(ec);
    const auto target = tmp / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    CHECK( filesystem::exists(tmp));
    auto res = download(hc, urls::url_view("/image"), {{}, {false}}, target);

    CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    CHECK(filesystem::exists(target));
    filesystem::remove(target, ec);
  }


  // SUBCASE("download-redirect")
  {
    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    auto res = download(hc, urls::url_view("/redirect-to?url=%2Fimage"), {{}, {false}}, target);

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

    CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    CHECK(filesystem::exists(target));
    error_code ec;
    filesystem::remove(target, ec);
  }


  // SUBCASE("download-too-many-redirects")
  {
    error_code ec;
    const auto target = filesystem::temp_directory_path() / "requests-test.html";
    if (filesystem::exists(target))
      filesystem::remove(target);
    auto res = download(hc,  urls::url_view("/redirect/10"), {{}, {false, requests::redirect_mode::private_domain, 3}}, target, ec);
    CHECK(res.history.size() == 3);

    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!filesystem::exists(target));
  }


  // SUBCASE("delete")
  {
    auto hdr = delete_(hc,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = patch(hc, urls::url_view("/patch"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // SUBCASE("patch-form")
  {
    auto hdr = patch(hc, urls::url_view("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  // SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = put(hc, urls::url_view("/put"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // SUBCASE("put-form")
  {
    auto hdr = put(hc, urls::url_view("/put"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  // SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = post(hc, urls::url_view("/post"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // SUBCASE("post-form")
  {
    auto hdr = post(hc, urls::url_view("/post"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }
}

TEST_CASE("sync-connection-request")
{
  SUBCASE("http") { http_request_connection(false);}
  SUBCASE("https") { http_request_connection(true);}
}


void run_tests(error_code ec,
               requests::connection & conn,
               urls::url_view url)
{
  namespace http = requests::http;
  namespace filesystem = requests::filesystem;

  check_ec(ec);
  async_request(
            conn,
            http::verb::get, urls::url_view("/headers"),
            requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}},
            tracker(
              [url](error_code ec, requests::response hdr)
              {
                // SUBCASE("headers")
                {

                  check_ec(ec);
                  auto hd = as_json(hdr).at("headers");

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }
              }));

  async_get(conn,
            urls::url_view("/get"),
            {requests::headers({{"Test-Header", "it works"}}), {false}},
            tracker(
              [url](error_code ec, requests::response hdr)
              {
                // SUBCASE("headers")
                {
                  check_ec(ec);
                  auto hd = as_json(hdr).at("headers");

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }
              }));

  async_get(conn, urls::url_view("/redirect-to?url=%2Fget"),
            {requests::headers({{"Test-Header", "it works"}}), {false}},
            tracker(
                [url](error_code ec, requests::response hdr)
                {
                  // SUBCASE("get-redirect")
                  {
                    CHECK(hdr.history.size() == 1u);
                    CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

                    auto hd = as_json(hdr).at("headers");

                    CHECK(hd.at("Host")        == json::value(url.host_name()));
                    CHECK(hd.at("Test-Header") == json::value("it works"));
                  }
                }));

    async_get(conn, urls::url_view("/redirect/10"),
              {{}, {false, requests::redirect_mode::private_domain, 5}},
              tracker(
                  [url](error_code ec, requests::response res)
                  {
                    // SUBCASE("too-many-redirects")
                    {
                      CHECK(res.history.size() == 5);
                      CHECK(res.headers.begin() == res.headers.end());
                      CHECK(ec == requests::error::too_many_redirects);
                    }
                  }));

    {
      auto pt = filesystem::temp_directory_path();
      const auto target = pt / "requests-test.png";
      if (filesystem::exists(target))
        filesystem::remove(target);

      CHECK(!filesystem::exists(target));
      async_download(conn, urls::url_view("/image"), {{}, {false}}, target,
                     tracker(
                         [url, target](error_code ec, requests::response_base res)
                         {
                           // SUBCASE("download")
                           {
                             CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
                             CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

                             CHECK(filesystem::exists(target));
                             filesystem::remove(target, ec);
                           }
                         }));
    }

    {
      const auto target = filesystem::temp_directory_path() / "requests-test-2.png";
      if (filesystem::exists(target))
        filesystem::remove(target);

      CHECK(!filesystem::exists(target));
      async_download(conn, urls::url_view("/redirect-to?url=%2Fimage"), {{}, {false}}, target,
                     tracker(
                       [url, target](error_code ec, requests::response_base res)
                       {
                         // SUBCASE("download-redirect")
                         {
                           CHECK(res.history.size() == 1u);
                           CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

                           CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
                           CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

                           CHECK(filesystem::exists(target));
                           filesystem::remove(target, ec);
                         }
                       }));

    }

    async_delete(conn,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}},
                 tracker(
                   [url](error_code ec, requests::response hdr)
                   {
                     // SUBCASE("delete")
                     {
                       auto js = as_json(hdr);
                       CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                       CHECK(js.at("headers").at("Content-Type") == "application/json");
                     }
                   }));


    async_patch(conn, urls::url_view("/patch"), json::value{{"test-key", "test-value"}}, {{}, {false}},
                tracker(
                   [url](error_code ec, requests::response hdr)
                   {
                     // SUBCASE("patch-json")
                     {
                       auto js = as_json(hdr);
                       CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                       CHECK(js.at("headers").at("Content-Type") == "application/json");
                     }
                   }));



    async_patch(conn, urls::url_view("/patch"),
                requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                {{}, {false}},
                tracker(
                    [url](error_code ec, requests::response hdr)
                    {
                      // SUBCASE("patch-form")
                      {
                        auto js = as_json(hdr);
                        CHECK(hdr.headers.result() == requests::http::status::ok);
                        CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                        CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
                      }
                    }));
    
    
    async_put(conn, urls::url_view("/put"), json::value{{"test-key", "test-value"}}, {{}, {false}},
                tracker(
                    [url](error_code ec, requests::response hdr)
                    {
                      // SUBCASE("put-json")
                      {
                        auto js = as_json(hdr);
                        CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                        CHECK(js.at("headers").at("Content-Type") == "application/json");
                      }
                    }));
    


    async_put(conn, urls::url_view("/put"),
                requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                {{}, {false}},
                tracker(
                    [url](error_code ec, requests::response hdr)
                    {
                      // SUBCASE("put-form")
                      {
                        auto js = as_json(hdr);
                        CHECK(hdr.headers.result() == requests::http::status::ok);
                        CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                        CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
                      }
                    }));
    
        
    async_post(conn, urls::url_view("/post"), json::value{{"test-key", "test-value"}}, {{}, {false}},
                tracker(
                    [url](error_code ec, requests::response hdr)
                    {
                      // SUBCASE("post-json")
                      {
                        auto js = as_json(hdr);
                        CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                        CHECK(js.at("headers").at("Content-Type") == "application/json");
                      }
                    }));
    


    async_post(conn, urls::url_view("/post"),
                requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                {{}, {false}},
                tracker(
                    [url](error_code ec, requests::response hdr)
                    {
                      // SUBCASE("post-form")
                      {
                        auto js = as_json(hdr);
                        CHECK(hdr.headers.result() == requests::http::status::ok);
                        CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                        CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
                      }
                    }));
}


TEST_CASE("async-connection-request")
{
  urls::url url;
  url.set_host(httpbin());
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};
  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();
  requests::connection conn(ctx, sslctx);

  auto do_the_thing = asio::deferred(
      [&](error_code ec, asio::ip::tcp::resolver::results_type res)
      {
         check_ec(ec);
         asio::ip::tcp::endpoint ep;
         if (!ec)
           ep = res.begin()->endpoint();
         else
           REQUIRE(!res.size());

         return asio::deferred.when(!ec)
             .then(conn.async_connect(ep, asio::deferred))
             .otherwise(asio::post(ctx, asio::append(asio::deferred, ec)));
      });

  SUBCASE("http")
  {
    conn = requests::connection(ctx);
    asio::ip::tcp::resolver rslvr{ctx};
    url.set_scheme("http");
    CHECK(!conn.uses_ssl());
    conn.set_host(url.encoded_host());

    rslvr.async_resolve(asio::string_view(url.encoded_host().data(), url.encoded_host().size()),
                        "80", do_the_thing)
                       (asio::append(&run_tests, std::ref(conn), urls::url_view(url)));
    ctx.run();
  }

  SUBCASE("https")
  {
    conn = requests::connection(ctx, sslctx);
    asio::ip::tcp::resolver rslvr{ctx};
    url.set_scheme("https");
    CHECK(conn.uses_ssl());
    conn.set_host(url.encoded_host());
    rslvr.async_resolve(asio::string_view(url.encoded_host().data(), url.encoded_host().size()),
                        "443", do_the_thing)
                       (asio::append(&run_tests, std::ref(conn), urls::url_view(url)));
    ctx.run();
  }
}

TEST_SUITE_END();