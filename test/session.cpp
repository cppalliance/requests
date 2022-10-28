// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/session.hpp>
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




TEST_SUITE_BEGIN("session");


struct http_maker
{
  urls::url url;
  http_maker(core::string_view target)
  {
    url = urls::parse_uri("http://" + httpbin() + std::string{target}).value();

  }

  operator urls::url_view () const
  {
    return url;
  }

  operator json::value () const
  {
    return json::string(url.buffer());
  }
};

struct https_maker
{
  urls::url url;
  https_maker(core::string_view target)
  {
    url = urls::parse_uri("https://" + httpbin() + std::string{target}).value();
  }

  operator urls::url_view () const
  {
    return url;
  }
  operator json::value () const
  {
    return json::string(url.buffer());
  }
};

TYPE_TO_STRING(http_maker);
TYPE_TO_STRING(https_maker);

TEST_CASE_TEMPLATE("sync-request", u, http_maker, https_maker)
{
  asio::io_context ctx;

  requests::session hc{ctx};
  hc.options().enforce_tls = false;
  hc.options().max_redirects = 5;

  SUBCASE("headers")
  {
    auto hdr = hc.request(beast::http::verb::get,
                          u("/headers"),
                          requests::empty{},
                          requests::headers({{"Test-Header", "it works"}}));

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  SUBCASE("get")
  {
    auto hdr = hc.get(u("/get"), requests::headers({{"Test-Header", "it works"}}));

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("get-redirect")
  {
    auto hdr = hc.get(u("/redirect-to?url=%2Fget"), requests::headers({{"Test-Header", "it works"}}));

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("too-many-redirects")
  {
    system::error_code ec;
    auto res = hc.get(u("/redirect/10"), {}, ec);
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
    auto res = hc.download(u("/image"), {}, target.string());

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
    auto res = hc.download(u("/redirect-to?url=%2Fimage"), {}, target.string());

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
    hc.options().max_redirects = 3;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);
    auto res = hc.download(u("/redirect/10"), {}, target.string(), ec);
    CHECK(res.history.size() == 2);

    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  }

  SUBCASE("delete")
  {
    auto hdr = hc.delete_(u("/delete"), json::value{{"test-key", "test-value"}}, {});

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.patch(u("/patch"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("patch-form")
  {
    auto hdr = hc.patch(u("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.put(u("/put"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("put-form")
  {
    auto hdr = hc.put(u("/put"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = hc.post(u("/post"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("post-form")
  {
    auto hdr = hc.post(u("/post"),
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

}


template<typename u>
asio::awaitable<void> async_http_pool_request()
{
  auto url = httpbin();
  using exec_t = typename asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
  requests::basic_session<exec_t> hc{co_await asio::this_coro::executor};
  hc.options().enforce_tls = false;
  hc.options().max_redirects = 3;

  auto headers = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto hdr = co_await hc.async_request(
        beast::http::verb::get, u("/headers"),
        requests::empty{}, requests::headers({{"Test-Header", "it works"}}));

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto get_ = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto h = requests::headers({{"Test-Header", "it works"}});
    auto hdr = co_await hc.async_get(u("/get"), h);

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };


  auto get_redirect = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto h = requests::headers({{"Test-Header", "it works"}});
    auto hdr = co_await hc.async_get(u("/redirect-to?url=%2Fget"), h);

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = hdr.json().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto too_many_redirects = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;

    auto res = co_await hc.async_get(u("/redirect/10"), {},
                                     asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 2);
    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);
    CHECK(ec == requests::error::too_many_redirects);
  };


  auto download = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    requests::request r{};
    auto res = co_await hc.async_download(u("/image"), {}, target.string());

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_redirect = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test-2.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = co_await hc.async_download(u("/redirect-to?url=%2Fimage"), {}, target.string());

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.header.at(beast::http::field::content_length)) > 0u);
    CHECK(res.string_view().empty());
    CHECK(res.header.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_too_many_redirects = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    requests::request r{{}, {false, requests::private_domain, 3}};
    auto res = co_await hc.async_download(u("/redirect/10"), {}, target.string(),
                                          asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 2);

    CHECK(beast::http::to_status_class(res.header.result()) == beast::http::status_class::redirection);

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  };


  auto delete_ = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value v{{"test-key", "test-value"}};
    auto hdr = co_await hc.async_delete(u("/delete"), v, {});

    auto js = hdr.json();
    CHECK(beast::http::to_status_class(hdr.header.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  };

  auto patch_json = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await hc.async_patch(u("/patch"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto patch_form = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_patch(u("/patch"), f, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto put_json = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await hc.async_put(u("/put"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto put_form = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_put(u("/put"), f, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto post_json = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};

    auto hdr = co_await hc.async_post(u("/post"), msg, {});

    auto js = hdr.json();
    CHECK(hdr.header.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto post_form = [](requests::basic_session<exec_t> & hc, core::string_view url) -> asio::awaitable<void>
  {
    requests::form f = {{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await hc.async_post(u("/post"), f, {});

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
}

TEST_CASE_TEMPLATE("async-session-request", M, http_maker, https_maker)
{
  auto url = httpbin();

  asio::io_context ctx;
  asio::co_spawn(ctx,
                 async_http_pool_request<M>(),
                 [](std::exception_ptr e)
                 {
                   CHECK(e == nullptr);
                 });

  ctx.run();
}

TEST_SUITE_END();