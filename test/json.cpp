//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/json.hpp>
#include <boost/requests/connection.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/optional/optional_io.hpp>

#include "doctest.h"
#include "extern.hpp"
#include "string_maker.hpp"


using namespace boost;

inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}


TYPE_TO_STRING(requests::http_connection);
TYPE_TO_STRING(requests::https_connection);


using aw_exec = asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
using aw_http_connection = requests::basic_http_connection<aw_exec>;
using aw_https_connection = requests::basic_https_connection<aw_exec>;

TYPE_TO_STRING(aw_http_connection);
TYPE_TO_STRING(aw_https_connection);


TEST_SUITE_BEGIN("json-connection");

TEST_CASE("ssl-detect")
{
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context::tlsv11};


  requests::http_connection conn{ctx};
  requests::https_connection sconn{ctx, sslctx};

  CHECK(requests::detail::get_ssl_layer(conn) == nullptr);
  CHECK(requests::detail::get_ssl_layer(sconn) == &sconn.next_layer());
}

template<typename Stream, typename Exec>
auto make_conn_impl(Exec && exec, asio::ssl::context & sslctx, std::false_type)
{
  return Stream(exec);
}

template<typename Stream, typename Exec>
auto make_conn_impl(Exec && exec, asio::ssl::context & sslctx, std::true_type)
{
  return Stream(exec, sslctx);
}


template<typename Stream, typename Exec>
auto make_conn(Exec && exec, asio::ssl::context & sslctx)
{
  return make_conn_impl<Stream>(std::forward<Exec>(exec), sslctx, requests::detail::has_ssl<Stream>{});
}

TEST_CASE_TEMPLATE("sync-https-request", Conn, requests::http_connection, requests::https_connection)
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  Conn hc = make_conn<Conn>(ctx.get_executor(), sslctx);
  hc.set_host(url);
  asio::ip::tcp::resolver rslvr{ctx};
  auto ep = *rslvr.resolve(url, requests::detail::has_ssl_v<Conn> ? "https" : "http").begin();

  hc.connect(ep);

  SUBCASE("headers")
  {
    auto hdr = request(hc, beast::http::verb::get, urls::url_view("/headers"),
                          requests::empty{},
                          {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("stream")
  {
    auto str = hc.ropen(beast::http::verb::get, urls::url_view("/get"),
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});

    json::stream_parser sp;

    char buf[32];

    system::error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = str.read_some(asio::buffer(buf), ec);
      CHECK(ec == system::error_code{});
      sp.write_some(buf, sz, ec);
      CHECK(ec == system::error_code{});
    }

    auto hd = sp.release().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("get-redirect")
  {
    auto hdr = requests::json::get(hc, urls::url_view("/redirect-to?url=%2Fget"),
                        {requests::headers({{"Test-Header", "it works"}}), {false}});

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = hdr.value.at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("too-many-redirects")
  {
    system::error_code ec;
    auto res = requests::json::get(hc, urls::url_view("/redirect/10"), {{}, {false, requests::private_domain, 5}}, ec);
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  SUBCASE("delete")
  {
    auto hdr = requests::json::delete_(hc,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto & js = hdr.value;
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    REQUIRE(js);
    CHECK(js->at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("patch")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::patch(hc, urls::url_view("/patch"), msg, {{}, {false}});

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::put(hc, urls::url_view("/put"), msg, {{}, {false}});

    auto & js = hdr.value;

    CHECK(hdr.headers.result() == beast::http::status::ok);
    REQUIRE(js);
    CHECK(js->at("headers").at("Content-Type") == "application/json");
    CHECK(js->at("json") == msg);
  }

  SUBCASE("post")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::post(hc, urls::url_view("/post"), msg, {{}, {false}});

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }
}



template<typename Conn>
asio::awaitable<void> async_https_request()
{
  auto url = httpbin();

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  auto hc = make_conn<Conn>(co_await asio::this_coro::executor, sslctx);
  hc.set_host(url);
  asio::ip::tcp::resolver rslvr{co_await asio::this_coro::executor};
  auto ep = *rslvr.resolve(url, requests::detail::has_ssl_v<Conn> ? "https" : "http").begin();
  co_await hc.async_connect(ep);

  auto get_ = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await requests::json::async_get(hc, urls::url_view("/get"), r);

    auto & hd = hdr.value.at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto get_redirect = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await requests::json::async_get(hc, urls::url_view("/redirect-to?url=%2Fget"), r);

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto & hd = hdr.value.at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto too_many_redirects = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    requests::request_settings r{{}, {false, requests::private_domain, 5}};

    auto res = co_await requests::json::async_get(hc, urls::url_view("/redirect/10"), r,
                                  asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  };

  auto delete_ = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{{}, {false}};
    json::value v{{"test-key", "test-value"}};
    auto hdr = co_await requests::json::async_delete(hc,  urls::url_view("/delete"), v, r);

    auto & js = hdr.value;
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  };

  auto patch_ = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request_settings r{{}, {false}};
    auto hdr = co_await requests::json::async_patch(hc, urls::url_view("/patch"), msg, r);

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto put_ = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request_settings r{{}, {false}};
    auto hdr = co_await requests::json::async_put(hc, urls::url_view("/put"), msg, r);

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    REQUIRE(js);

    CHECK(js->at("headers").at("Content-Type") == "application/json");
    CHECK(js->at("json") == msg);
  };

  auto post_ = [](Conn & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};

    requests::request_settings r{{}, {false}};
    auto hdr = co_await requests::json::async_post(hc, urls::url_view("/post"), msg, r);

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };


  using namespace asio::experimental::awaitable_operators ;
  co_await (
         get_(hc, url)
      && get_redirect(hc, url)
      && too_many_redirects(hc, url)
      && delete_(hc, url)
      && patch_(hc, url)
      && put_(hc, url)
      && post_(hc, url)
  );
}



TEST_CASE_TEMPLATE("async-https-request", Conn, aw_http_connection, aw_https_connection)
{
  auto url = httpbin();

  asio::io_context ctx;
  asio::co_spawn(ctx,
                 async_https_request<Conn>(),
                 [](std::exception_ptr e)
                 {
                   CHECK(e == nullptr);
                 });

  ctx.run();
}

TEST_SUITE_END();