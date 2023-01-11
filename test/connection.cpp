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
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "doctest.h"
#include "string_maker.hpp"

using namespace boost;

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


  SUBCASE("stream-all")
  {
    auto str = hc.ropen(beast::http::verb::get, urls::url_view("/get"),
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});

    std::string buf;
    auto bb = asio::dynamic_buffer(buf);

    system::error_code ec;
    auto sz = str.read(bb, ec);

    CHECK(ec == system::error_code{});

    auto val = json::parse(buf);

    auto hd = val.at("headers");
    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  SUBCASE("stream-dump")
  {
    auto str = hc.ropen(beast::http::verb::get, urls::url_view("/get"), 
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});
    str.dump();
  }

  SUBCASE("get")
  {
    auto hdr = get(hc, urls::url_view("/get"), {requests::headers({{"Test-Header", "it works"}}), {false}});

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("get-redirect")
  {
    auto hdr = get(hc, urls::url_view("/redirect-to?url=%2Fget"),
                   {requests::headers({{"Test-Header", "it works"}}), {false}});

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("too-many-redirects")
  {
    system::error_code ec;
    auto res = get(hc, urls::url_view("/redirect/10"), {{}, {false, requests::redirect_mode::private_domain, 5}}, ec);
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  SUBCASE("download")
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = download(hc, urls::url_view("/image"), {{}, {false}}, target.string());

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

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
    auto res = download(hc, urls::url_view("/redirect-to?url=%2Fimage"), {{}, {false}}, target.string());

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

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
    auto res = download(hc,  urls::url_view("/redirect/10"), {{}, {false, requests::redirect_mode::private_domain, 3}}, target.string(), ec);
    CHECK(res.history.size() == 3);

    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  }


  SUBCASE("delete")
  {
    auto hdr = delete_(hc,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = patch(hc, urls::url_view("/patch"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("patch-form")
  {
    auto hdr = patch(hc, urls::url_view("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = put(hc, urls::url_view("/put"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("put-form")
  {
    auto hdr = put(hc, urls::url_view("/put"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = post(hc, urls::url_view("/post"), msg, {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("post-form")
  {
    auto hdr = post(hc, urls::url_view("/post"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {{}, {false}});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }
}

TEST_CASE("sync-connection-request")
{
  SUBCASE("http") { http_request_connection(false);}
  SUBCASE("https") { http_request_connection(true);}
}

asio::awaitable<void> async_connection_request(bool https)
{
  auto url = httpbin();
  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  requests::connection::defaulted<asio::use_awaitable_t<>> hc =
            https ? requests::connection(co_await asio::this_coro::executor, sslctx) :
                    requests::connection(co_await asio::this_coro::executor);
  CHECK(hc.uses_ssl() == https);
  hc.set_host(url);
  asio::ip::tcp::resolver rslvr{co_await asio::this_coro::executor};
  asio::ip::tcp::endpoint ep = *rslvr.resolve(url, https ? "https" : "http").begin();

  co_await hc.async_connect(ep);

  auto headers = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{requests::headers({{"Test-Header", "it works"}}), {false}};

    auto hdr = co_await async_request(hc,
        beast::http::verb::get, urls::url_view("/headers"),
        requests::empty{}, r);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto get_ = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await async_get(hc, urls::url_view("/get"), r);

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto stream = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto str = co_await hc.async_ropen(beast::http::verb::get, urls::url_view("/get"), requests::empty{},
                                       {requests::headers({{"Test-Header", "it works"}}), {false}});

    json::stream_parser sp;
    char buf[32];

    system::error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = co_await str.async_read_some(asio::buffer(buf));
      CHECK(ec == system::error_code{});
      sp.write_some(buf, sz, ec);
      CHECK(ec == system::error_code{});
    }

    auto hd = sp.release().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };


  auto stream_full = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto str = co_await hc.async_ropen(beast::http::verb::get, urls::url_view("/get"), requests::empty{},
                                       {requests::headers({{"Test-Header", "it works"}}), {false}});

    std::string ss;
    auto buf = asio::dynamic_buffer(ss);

    system::error_code ec;
    auto sz = co_await str.async_read(buf);

    auto val = json::parse(ss);
    auto hd = val.at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto stream_dump = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto str = co_await hc.async_ropen(beast::http::verb::get, urls::url_view("/get"), requests::empty{},
                                       {requests::headers({{"Test-Header", "it works"}}), {false}});
    co_await str.async_dump();

  };

  auto get_redirect = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{requests::headers({{"Test-Header", "it works"}}), {false}};
    auto hdr = co_await async_get(hc, urls::url_view("/redirect-to?url=%2Fget"), r);

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto too_many_redirects = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    requests::request_settings r{{}, {false, requests::redirect_mode::private_domain, 5}};

    auto res = co_await async_get(hc, urls::url_view("/redirect/10"), r,
                                  asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  };


  auto download = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    requests::request_settings r{{}, {false}};
    auto res = co_await async_download(hc, urls::url_view("/image"), r, target.string());

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_redirect = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test-2.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    requests::request_settings r{{}, {false}};
    auto res = co_await async_download(hc, urls::url_view("/redirect-to?url=%2Fimage"), r, target.string());

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_too_many_redirects = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    requests::request_settings r{{}, {false, requests::redirect_mode::private_domain, 3}};
    auto res = co_await async_download(hc,  urls::url_view("/redirect/10"), r, target.string(),
                                       asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 3);
      CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  };

  auto delete_ = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{{}, {false}};
    json::value v{{"test-key", "test-value"}};
    auto hdr = co_await async_delete(hc,  urls::url_view("/delete"), v, r);

    auto js = as_json(hdr);
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  };

  auto patch_json = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request_settings r{{}, {false}};
    auto hdr = co_await async_patch(hc, urls::url_view("/patch"), msg, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto patch_form = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{{}, {false}};
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await async_patch(hc, urls::url_view("/patch"), f, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto put_json = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    requests::request_settings r{{}, {false}};
    auto hdr = co_await async_put(hc, urls::url_view("/put"), msg, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto put_form = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{{}, {false}};
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await async_put(hc, urls::url_view("/put"), f, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto post_json = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};

    requests::request_settings r{{}, {false}};
    auto hdr = co_await async_post(hc, urls::url_view("/post"), msg, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto post_form = [](requests::connection::defaulted<asio::use_awaitable_t<>> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::request_settings r{{}, {false}};
    requests::form f = {{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await async_post(hc, urls::url_view("/post"), f, r);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  using namespace asio::experimental::awaitable_operators ;
  co_await (
      headers(hc, url)
      && get_(hc, url)
      && stream(hc, url)
      && stream_dump(hc, url)
      && stream_full(hc, url)
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
}


TEST_CASE("async-connection-request")
{
  SUBCASE("http")
  {
    asio::io_context ctx;
    asio::co_spawn(ctx,
                   async_connection_request(false),
                   [](std::exception_ptr e)
                   {
                     CHECK(e == nullptr);
                   });

    ctx.run();
  }

  SUBCASE("https")
  {
    asio::io_context ctx;
    asio::co_spawn(ctx,
                   async_connection_request(true),
                   [](std::exception_ptr e)
                   {
                     CHECK(e == nullptr);
                   });

    ctx.run();
  }

}

TEST_SUITE_END();