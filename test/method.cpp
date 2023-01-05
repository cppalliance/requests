// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/json.hpp>
#include <boost/requests/download.hpp>
#include <boost/requests/form.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request.hpp>

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



TEST_SUITE_BEGIN("free");


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
  requests::default_options().enforce_tls = false;
  requests::default_options().max_redirects = 5;

  SUBCASE("headers")
  {
    auto hdr = requests::request(beast::http::verb::get,
                          u("/headers"),
                          requests::empty{},
                          requests::headers({{"Test-Header", "it works"}}));

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  SUBCASE("get")
  {
    auto hdr = requests::get(u("/get"), requests::headers({{"Test-Header", "it works"}}));

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("get-redirect")
  {
    auto hdr = requests::get(u("/redirect-to?url=%2Fget"), requests::headers({{"Test-Header", "it works"}}));

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  SUBCASE("too-many-redirects")
  {
    system::error_code ec;
    requests::default_session().options().max_redirects = 3;
    auto res = requests::get(u("/redirect/10"), {}, ec);
    CHECK(res.history.size() == 3);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  SUBCASE("download")
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = requests::download(u("/image"), {}, target.string());

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
    auto res = requests::download(u("/redirect-to?url=%2Fimage"), {}, target.string());

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
    requests::default_options().max_redirects = 3;
    requests::default_session().options().max_redirects = 3;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);
    auto res = requests::download(u("/redirect/10"), {}, target.string(), ec);
    CHECK(res.history.size() == 3);

    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  }

  SUBCASE("delete")
  {
    auto hdr = requests::delete_(u("/delete"), json::value{{"test-key", "test-value"}}, {});

    auto js = as_json(hdr);
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  SUBCASE("patch-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::patch(u("/patch"), msg, {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("patch-form")
  {
    auto hdr = requests::patch(u("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("put-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::put(u("/put"), msg, {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("put-form")
  {
    auto hdr = requests::put(u("/put"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  SUBCASE("post-json")
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::post(u("/post"), msg, {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  SUBCASE("post-form")
  {
    auto hdr = requests::post(u("/post"),
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {});

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

}


template<typename u>
asio::awaitable<void> async_http_pool_request()
{
  auto url = httpbin();
  requests::default_options().enforce_tls = false;
  requests::default_options().max_redirects = 3;

  auto headers = [](core::string_view url) -> asio::awaitable<void>
  {
    auto hdr = co_await requests::async_request(
        beast::http::verb::get, u("/headers"),
        requests::empty{}, requests::headers({{"Test-Header", "it works"}}),
        asio::use_awaitable);

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto get_ = [](core::string_view url) -> asio::awaitable<void>
  {
    auto h = requests::headers({{"Test-Header", "it works"}});
    auto hdr = co_await requests::async_get(u("/get"), h,
                                            asio::use_awaitable);

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };


  auto get_redirect = [](core::string_view url) -> asio::awaitable<void>
  {
    auto h = requests::headers({{"Test-Header", "it works"}});
    auto hdr = co_await requests::async_get(u("/redirect-to?url=%2Fget"), h,
                                            asio::use_awaitable);

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  };

  auto too_many_redirects = [](core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;

    auto res = co_await requests::async_get(u("/redirect/10"), {},
                                     asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 3);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  };


  auto download = [](core::string_view url) -> asio::awaitable<void>
  {
    auto pt = std::filesystem::temp_directory_path();

    const auto target = pt / "requests-test.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = co_await requests::async_download(u("/image"), {}, target.string(),
                                                 asio::use_awaitable);

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_redirect = [](core::string_view url) -> asio::awaitable<void>
  {
    const auto target = std::filesystem::temp_directory_path() / "requests-test-2.png";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    CHECK(!std::filesystem::exists(target));
    auto res = co_await requests::async_download(u("/redirect-to?url=%2Fimage"), {}, target.string(),
                                                 asio::use_awaitable);

    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(beast::http::field::location) == "/image");

    CHECK(std::stoull(res.headers.at(beast::http::field::content_length)) > 0u);
    CHECK(res.headers.at(beast::http::field::content_type) == "image/png");

    CHECK(std::filesystem::exists(target));
    std::error_code ec;
    std::filesystem::remove(target, ec);
  };


  auto download_too_many_redirects = [](core::string_view url) -> asio::awaitable<void>
  {
    system::error_code ec;
    const auto target = std::filesystem::temp_directory_path() / "requests-test.html";
    if (std::filesystem::exists(target))
      std::filesystem::remove(target);

    auto res = co_await requests::async_download(u("/redirect/10"), {}, target.string(),
                                          asio::redirect_error(asio::use_awaitable, ec));
    CHECK(res.history.size() == 3);
    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!std::filesystem::exists(target));
  };


  auto delete_ = [](core::string_view url) -> asio::awaitable<void>
  {
    json::value v{{"test-key", "test-value"}};
    auto hdr = co_await requests::async_delete(u("/delete"), v, {},
                                               asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  };

  auto patch_json = [](core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await requests::async_patch(u("/patch"), msg, {},
                                              asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto patch_form = [](core::string_view url) -> asio::awaitable<void>
  {
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await requests::async_patch(u("/patch"), f, {},
                                              asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto put_json = [](core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = co_await requests::async_put(u("/put"), msg, {},
                                            asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto put_form = [](core::string_view url) -> asio::awaitable<void>
  {
    requests::form f{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await requests::async_put(u("/put"), f, {},
                                            asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  auto post_json = [](core::string_view url) -> asio::awaitable<void>
  {
    json::value msg {{"test-key", "test-value"}};

    auto hdr = co_await requests::async_post(u("/post"), msg, {},
                                             asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  };

  auto post_form = [](core::string_view url) -> asio::awaitable<void>
  {
    requests::form f = {{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}};
    auto hdr = co_await requests::async_post(u("/post"), f, {},
                                             asio::use_awaitable);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  };

  using namespace asio::experimental::awaitable_operators ;
  co_await (
      headers(url)
      && get_(url)
      && get_redirect(url)
      && too_many_redirects(url)
      && download(url)
      && download_redirect(url)
      && download_too_many_redirects(url)
      && delete_(url)
      && patch_json(url)
      && patch_form(url)
      && put_json(url)
      && put_form(url)
      && post_json(url)
      && post_form(url)
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