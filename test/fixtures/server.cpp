//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

using namespace boost;

std::size_t test_server::cnt_ = 0u;

template<typename Acceptor>
void test_server::run_(Acceptor acc, boost::asio::yield_context ctx)
{
  while (ctx.cancelled() == asio::cancellation_type::none)
  {
    auto sock = acc.async_accept(ctx);
    asio::spawn(
        ctx,
        [this, s = std::move(sock)](asio::yield_context ctx) mutable
        {
          run_session(std::move(s), ctx);
        },
        asio::detached);
  }
}




struct char_chunked_string_transfer
{
  beast::http::string_body inner;

  using value_type = std::string;


  class writer
  {
    value_type const& body_;
    value_type::const_iterator itr_{body_.begin()};

  public:
    using const_buffers_type =
        asio::const_buffer;

    template<bool isRequest, class Fields>
    explicit
        writer(beast::http::header<isRequest, Fields> const&, value_type const& b)
        : body_(b)
    {
    }

    void
    init(system::error_code& ec)
    {
      ec = {};
    }

    boost::optional<std::pair<const_buffers_type, bool>>
    get(system::error_code& ec)
    {
      ec = {};
      if (itr_ == body_.end())
        return {{const_buffers_type{} , false}};
      else
        return {{const_buffers_type{&*itr_++, 1u} , true}};
    }
  };
};

template<typename Socket>
void test_server::run_session(Socket sock, boost::asio::yield_context yield_)
{
  namespace http = boost::beast::http;
  boost::beast::flat_buffer buf;

  boost::system::error_code ec;
  auto yield = yield_[ec];

  while (sock.is_open() && !ec)
  {

    http::request<http::string_body> req;
    http::async_read(sock, buf, req, yield);
    if (ec)
      return;


    if (req.target() == "/echo")
    {
      http::response<http::string_body> res{http::status::ok, req.version()};
      res.set("Requests-Method", req.method_string());
      res.body() = std::move(req.body());

      for (auto & hd : req)
        if (hd.name() == beast::http::field::unknown)
          res.set(hd.name_string(), hd.value());

      res.prepare_payload();

      http::async_write(sock, res, yield);
    }
    else if (req.target() == "/echo-chunked")
    {
      http::response<char_chunked_string_transfer> res{http::status::ok, req.version()};
      http::response_serializer<char_chunked_string_transfer> ser{res};
      res.set("Requests-Method", req.method_string());
      res.body() = std::move(req.body());

      for (auto & hd : req)
        if (hd.name() == beast::http::field::unknown)
          res.set(hd.name_string(), hd.value());

      res.prepare_payload();
      http::async_write(sock, ser, yield);
    }
    else if (req.target() == "/ws/echo")
    {
      if (beast::websocket::is_upgrade(req))
      {
        namespace ws = boost::beast::websocket;
        ws::stream<Socket&> wws{sock};
        wws.async_accept(req, yield);
        wws.async_write(boost::asio::buffer(core::string_view("Hello World!")), yield);
      }
      else
      {
        http::response<http::string_body> res{http::status::upgrade_required, req.version()};
        res.set("Requests-Method", req.method_string());
        res.body() = std::move(req.body());

        for (auto & hd : req)
          if (hd.name() == beast::http::field::unknown)
            res.set(hd.name_string(), hd.value());

        res.prepare_payload();
        http::async_write(sock, res, yield);
      }
    }
    else if(req.target().starts_with("/redirect/"))
    {
      auto n = std::stoi(req.target().substr(10));

      if (n == 0)
      {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set("Requests-Method", req.method_string());

        for (auto & hd : req)
          if (hd.name() == beast::http::field::unknown)
            res.set(hd.name_string(), hd.value());


        res.body() = "Hello World!";
        res.prepare_payload();
        http::async_write(sock, res, yield);
      }
      else
      {
        http::response<http::string_body> res{http::status::permanent_redirect, req.version()};
        res.set("Requests-Method", req.method_string());
        res.set(http::field::location, "/redirect/" + std::to_string(--n));

        res.body() = "Redirecting...";

        for (auto & hd : req)
          if (hd.name() == beast::http::field::unknown)
            res.set(hd.name_string(), hd.value());

        res.prepare_payload();
        http::async_write(sock, res, yield);
      }
    }
    else if(req.target() == "/invalid-redirect")
    {
      http::response<http::empty_body> res{http::status::permanent_redirect, req.version()};
      res.set("Requests-Method", req.method_string());

      for (auto & hd : req)
        if (hd.name() == beast::http::field::unknown)
          res.set(hd.name_string(), hd.value());

      res.prepare_payload();
      http::async_write(sock, res, yield);
    }
    else if(req.target() == "/boost-redirect")
    {
      http::response<http::string_body> res{http::status::permanent_redirect, req.version()};
      res.set("Requests-Method", req.method_string());
      res.set(http::field::location, "http://boost.org");

      for (auto & hd : req)
        if (hd.name() == beast::http::field::unknown)
          res.set(hd.name_string(), hd.value());

      res.body() = "Redirecting...";
      res.prepare_payload();
      http::async_write(sock, res, yield);
    }
    else if(req.target() == "/set-cookie")
    {
      http::response<http::empty_body> res{http::status::permanent_redirect, req.version()};
      res.set("Requests-Method", req.method_string());
      res.set(http::field::location, "http://boost.org");

      for (auto & hd : req)
        if (hd.name() == beast::http::field::unknown)
          res.set(hd.name_string(), hd.value());

      res.prepare_payload();
      http::async_write(sock, res, yield);
    }
  }
}

test_server::test_server()
{
  namespace asio = boost::asio;
  namespace filesystem = boost::requests::filesystem;
  auto tmp = filesystem::temp_directory_path();
  path_ = (tmp / ("boost_request_test_socket_"
                  + std::to_string(boost::process::v2::current_pid())
                  + std::to_string(cnt_++))).string();

  auto ep = boost::asio::local::stream_protocol::endpoint{path_};
  ep_ = ep;
  {
    asio::local::stream_protocol::acceptor acc{tp_.get_executor(), ep};
    acc.listen();
    asio::spawn(
        tp_,
        [this, ep, acc = std::move(acc)](auto ctx) mutable
        {
          run_(std::move(acc), ctx);
        }, boost::asio::detached);
  }
}

