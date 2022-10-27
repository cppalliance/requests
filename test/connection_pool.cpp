// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection_pool.hpp>
#include <boost/requests/form.hpp>
#include <boost/json.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "doctest.h"
#include "string_maker.hpp"

#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost;

inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}


TYPE_TO_STRING(requests::http_connection_pool);
TYPE_TO_STRING(requests::https_connection_pool);


using aw_exec = asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
using aw_http_connection_pool = requests::basic_http_connection_pool<aw_exec>;
using aw_https_connection_pool = requests::basic_https_connection_pool<aw_exec>;


static_assert(!requests::detail::has_ssl_v<requests::http_connection>);
static_assert(requests::detail::has_ssl_v<requests::https_connection>);
TYPE_TO_STRING(aw_http_connection_pool);
TYPE_TO_STRING(aw_https_connection_pool);


TEST_SUITE_BEGIN("connection-pool");

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
auto make_conn_pool_impl(Exec && exec, asio::ssl::context & sslctx, std::false_type)
{
  return Stream(exec);
}

template<typename Stream, typename Exec>
auto make_conn_pool_impl(Exec && exec, asio::ssl::context & sslctx, std::true_type)
{
  return Stream(exec, sslctx);
}


template<typename Pool, typename Exec>
auto make_conn_pool(Exec && exec, asio::ssl::context & sslctx)
{
  return make_conn_pool_impl<Pool>(std::forward<Exec>(exec), sslctx,
                                   requests::detail::has_ssl<typename Pool::connection_type>{});
}

TEST_CASE_TEMPLATE("sync-request", Pool,
                   requests::http_connection_pool,
                   requests::https_connection_pool)
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  Pool hc = make_conn_pool<Pool>(ctx.get_executor(), sslctx);
  hc.lookup(urls::parse_authority(url).value());


  SUBCASE("headers")
  {
    auto hdr = hc.request(beast::http::verb::get, "/headers",
                          requests::empty{},
                          {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  SUBCASE("get")
  {
    auto hdr = hc.get("/get", {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("get-redirect")
  {
    auto hdr = hc.get("/redirect-to?url=%2Fget", {requests::headers({{"Test-Header", "it works"}}), {false}});

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("too-many-redirects")
  {
    system::error_code ec;
    auto res = hc.get("/redirect/10", {{}, {false, requests::private_domain, 5}}, ec);
    CHECK(res.history.size() == 4);
    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);
    CHECK(ec == requests::error::too_many_redirects);
  }

  SUBCASE("download")
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test.png";
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


  SUBCASE("download-redirect")
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = hc.download("/redirect-to?url=%2Fimage", {{}, {false}}, target.string());

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  }


  SUBCASE("download-too-many-redirects")
  {
    system::error_code ec;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);
    auto res = hc.download("/redirect/10", {{}, {false, requests::private_domain, 3}}, target.string(), ec);
    CHECK(res.history.size() == 2);

    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  }

   SUBCASE("delete")
  {
    auto hdr = hc.delete_("/delete", json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
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


template<typename Pool>
asio::awaitable<void> async_http_pool_request()
{
  auto url = httpbin();

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  auto hc = make_conn_pool<Pool>(co_await asio::this_coro::executor, sslctx);
  co_await hc.async_lookup(urls::parse_authority(url).value());

  auto headers = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await hc.async_request(
                          beast::http::verb::get, "/headers",
                          requests::empty{}, r);

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto get_ = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await hc.async_get("/get", r);

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };


  auto get_redirect = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await hc.async_get("/redirect-to?url=%2Fget", r);

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto too_many_redirects = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    requests::request r{{}, {false, requests::private_domain, 5}};

    auto res = co_await hc.async_get("/redirect/10", r,
                                     asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 4);
    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);
    CHECK(ec == requests::error::too_many_redirects);
  };


  auto download = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    requests::request r{{}, {false}};
    auto res = co_await hc.async_download("/image", r, target.string());

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_redirect = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test-2.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    requests::request r{{}, {false}};
    auto res = co_await hc.async_download("/redirect-to?url=%2Fimage", r, target.string());

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_too_many_redirects = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    requests::request r{{}, {false, requests::private_domain, 3}};
    auto res = co_await hc.async_download("/redirect/10", r, target.string(),
                                          asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 2);

    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  };


  auto delete_ = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{{}, {false}};
    json::value v{{"test-key", "test-value"}};
    auto hdr = co_await hc.async_delete("/delete", v, r);

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  };

  auto patch_json = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request r{{}, {false}};
    auto hdr = co_await hc.async_patch("/patch", msg, r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto patch_form = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{{}, {false}};
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_patch("/patch",
                        f,
                        r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto put_json = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request r{{}, {false}};
    auto hdr = co_await hc.async_put("/put", msg, r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto put_form = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request r{{}, {false}};
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_put("/put",
                      f, r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto post_json = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};

    requests::request r{{}, {false}};
    auto hdr = co_await hc.async_post("/post", msg, r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto post_form = [](Pool & hc, core::string_view url) -> asio::awaitable<void>
  {

    requests::request r{{}, {false}};
    requests::form f = {{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_post("/post", f, r);

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  using namespace asio::experimental::awaitable_operators ;
  co_await (
         headers(hc, url)
      && get_(hc, url)
      && get_redirect(hc, url)
      && too_many_redirects(hc, url)
      && download(hc, url)
      && download_redirect(hc, url)
      && download_too_many_redirects(hc, url)
      && delete_(hc, url)
      && patch_json(hc, url)
      && patch_form(hc, url)
      && put_json(hc, url)
      && put_form(hc, url)
      && post_json(hc, url)
      && post_form(hc, url)
  );
  co_await asio::post(asio::use_awaitable);

  CHECK(hc.limit() == hc.active());
}

TEST_CASE_TEMPLATE("async-request", Pool, aw_http_connection_pool, aw_https_connection_pool)
{
  auto url = httpbin();

  asio::io_context ctx;
  asio::co_spawn(ctx,
                 async_http_pool_request<Pool>(),
                 [](std::exception_ptr e)
                 {
                   printf("Result : %p\n", *reinterpret_cast<void**>(&e));
                   CHECK(e == nullptr);
                 });

  ctx.run();
}

TEST_SUITE_END();