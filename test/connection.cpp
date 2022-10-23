// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
#include <boost/requests/form.hpp>
#include <boost/json.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "doctest.h"
#include "string_maker.hpp"

#include <boost/asio/ssl/host_name_verification.hpp>

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


TEST_SUITE_BEGIN("connection");

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
  SUBCASE("single-request")
  {
    beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
    beast::http::response<beast::http::string_body> res;

    hc.single_request(req, res);

    auto js = json::parse(res.body());

    CHECK(js.at("headers").at("Host").as_string() == url);
    CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);

  }

  SUBCASE("headers")
  {
    auto hdr = hc.request(beast::http::verb::get, "/headers",
                          requests::empty{},
                          {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  SUBCASE("download")
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    printf("Target : %s\n", target.c_str());
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = hc.download("/image", {{}, {false}}, target.string());

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  }


  SUBCASE("delete")
  {
    auto hdr = hc.delete_("/delete", json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("get")
  {
    auto hdr = hc.get("/get", {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.patch("/patch", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("patch-form")
  {
    auto hdr = hc.patch("/patch",
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.put("/put", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("put-form")
  {
    auto hdr = hc.put("/put",
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }
  
  SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.post("/post", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("post-form")
  {
    auto hdr = hc.post("/post",
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

}



asio::awaitable<void> async_http_exception()
{
  auto url = httpbin();

  using exec_t = typename asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
  exec_t exec{co_await asio::this_coro::executor};

  requests::basic_http_connection<exec_t> hc{exec};;
  hc.set_host(url);

  typename asio::ip::tcp::resolver::rebind_executor<exec_t>::other rslvr{exec};
  auto ep = *(co_await rslvr.async_resolve(url, "http")).begin();

  co_await hc.async_connect(ep);

  beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
  beast::http::response<beast::http::string_body> res;

  co_await hc.async_single_request(req, res);

  auto js = json::parse(res.body());

  CHECK(js.at("headers").at("Host").as_string() == url);
  CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);
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
  SUBCASE("single-request")
  {
    beast::http::request<beast::http::empty_body> req{beast::http::verb::get, "/headers", 11};
    beast::http::response<beast::http::string_body> res;

    co_await hc.async_single_request(req, res);

    auto js = json::parse(res.body());

    CHECK(js.at("headers").at("Host").as_string() == url);
    CHECK(js.at("headers").at("User-Agent").as_string() == "Requests-" BOOST_BEAST_VERSION_STRING);

  }

  SUBCASE("headers")
  {
    requests::request r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await hc.async_request(
                          beast::http::verb::get, "/headers",
                          requests::empty{}, r);

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("download")
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = co_await hc.async_download("/image", {{}, {false}}, target.string());

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  }


  SUBCASE("delete")
  {
    auto hdr = co_await hc.async_delete("/delete", json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("get")
  {
    auto hdr = co_await hc.async_get("/get", {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await hc.async_patch("/patch", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("patch-form")
  {
    auto hdr = co_await hc.async_patch("/patch",
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await hc.async_put("/put", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("put-form")
  {
    auto hdr = co_await hc.async_put("/put",
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await hc.async_post("/post", msg, {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("post-form")
  {
    auto hdr = co_await hc.async_post("/post",
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {{}, {false}});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }
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