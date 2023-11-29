//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/connection_pool.hpp>
#include <boost/requests/detail/connection_impl.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/version.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

auto connection_impl::ropen(beast::http::verb method,
                       urls::pct_string_view path,
                       http::fields & headers,
                       source & src,
                       cookie_jar * jar) -> stream
{
  system::error_code ec;
  auto res = ropen(method, path, headers, src, jar, ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}

auto connection_impl::ropen(beast::http::verb method,
                       urls::pct_string_view path,
                       http::fields & headers,
                       source & src,
                       cookie_jar * jar,
                       system::error_code & ec) -> stream
{
  const auto is_secure = use_ssl_;
  if (jar)
  {
    auto cc = jar->get(host(), is_secure, path);
    if (!cc.empty())
      headers.set(http::field::cookie, cc);
    else
      headers.erase(http::field::cookie);
  }
  else
    headers.erase(http::field::cookie);

  headers.set(http::field::host, host_);
  if (headers.count(http::field::user_agent) ==  0)
    headers.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);


  // write impl
  {
    if (!is_open())
    {
      next_layer_.next_layer().connect(endpoint_, ec);
      if (use_ssl_ && !ec)
        next_layer_.handshake(asio::ssl::stream_base::client, ec);
      if (ec)
        return stream{get_executor(), nullptr};
    }

    if (use_ssl_)
      write_request(next_layer_,              method, path, headers, src);
    else
      write_request(next_layer_.next_layer(), method, path, headers, src);

    if (ec)
      return stream{get_executor(), nullptr};
  }
  // write end

  if (ec)
    return stream{get_executor(), nullptr};

  stream str{get_executor(), shared_from_this()};
  str.parser_ = std::make_unique<beast::http::response_parser<beast::http::buffer_body>>(http::response_header{http::fields(headers.get_allocator())});
  str.parser_->body_limit(boost::none);
  str.parser_->on_chunk_body(handle_chunked_);

  if (method == http::verb::head || method == http::verb::connect)
    str.parser_->skip(true);

  if (use_ssl_)
    beast::http::read_header(next_layer_, buffer_, *str.parser_, ec);
  else
    beast::http::read_header(next_layer_.next_layer(), buffer_, *str.parser_, ec);

  if (ec)
    return stream{get_executor(), nullptr};

  if (jar)
  {
    auto cookie_itr = str.headers().find(http::field::set_cookie);
    if (cookie_itr != str.headers().end())
    {
      auto f = requests::parse_set_cookie_field(cookie_itr->value());
      if (f)
        jar->set(*f, host_);
      else
      {
        ec = f.error();
        return str;
      }
    }
  }
  return str;
}


void connection_impl::set_host(core::string_view sv, system::error_code & ec)
{
  next_layer_.set_verify_callback(asio::ssl::host_name_verification(host_ = sv), ec);
}


struct connection_impl::async_ropen_op
    : boost::asio::coroutine
{
  constexpr static const char * op_name = "connection_impl::async_ropen_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  std::shared_ptr<connection_impl> this_;

  optional<stream> str;
  beast::http::verb method;
  urls::pct_string_view path;
  http::fields & headers;
  source & src;
  cookie_jar * jar{nullptr};

  async_ropen_op(std::shared_ptr<connection_impl> this_,
                 beast::http::verb method,
                 urls::pct_string_view path,
                 http::fields & headers,
                 source & src,
                 cookie_jar * jar)
      : this_(std::move(this_)), method(method), path(path), headers(headers), src(src)
      , jar(jar)
  {
  }

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t res_ = 0u);
};


template<typename Self>
void connection_impl::async_ropen_op::operator()(
            Self && self,
            system::error_code ec, std::size_t)
{
  BOOST_ASIO_HANDLER_LOCATION((__FILE__, __LINE__, op_name));

  BOOST_ASIO_CORO_REENTER(this)
  {
    if (jar)
    {

      auto cc = jar->get(this_->host(), this_->use_ssl_, path);
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
      else
        headers.erase(http::field::cookie);
    }
    else
      headers.erase(http::field::cookie);

    headers.set(http::field::host, this_->host_);
    if (headers.count(http::field::user_agent) == 0)
      headers.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    if (!this_->is_open())
    {
      BOOST_REQUESTS_YIELD this_->next_layer_.next_layer().async_connect(this_->endpoint_, std::move(self));
      if (!ec && this_->use_ssl_)
      {
        BOOST_REQUESTS_YIELD this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
      }
      if (ec)
        break;
    }


    if (this_->use_ssl_)
    {
      BOOST_REQUESTS_YIELD async_write_request(this_->next_layer_, method, path, headers, src, std::move(self));
    }
    else
    {
      BOOST_REQUESTS_YIELD async_write_request(this_->next_layer_.next_layer(), method, path, headers, src, std::move(self));
    }

    if (ec)
      break;

    str.emplace(this_->get_executor(), this_); // , req.get_allocator().resource()
    str->parser_ = std::make_unique<beast::http::response_parser<beast::http::buffer_body>>(http::response_header{http::fields(headers.get_allocator())});
    str->parser_->body_limit(boost::none);
    str->parser_->on_chunk_body(this_->handle_chunked_);

    if (method == http::verb::head || method == http::verb::connect)
      str->parser_->skip(true);

    if (this_->use_ssl_)
    {
      BOOST_REQUESTS_YIELD beast::http::async_read_header(this_->next_layer_, this_->buffer_, *str->parser_, std::move(self));
    }
    else
    {
      BOOST_REQUESTS_YIELD beast::http::async_read_header(this_->next_layer_.next_layer(), this_->buffer_, *str->parser_, std::move(self));
    }

    if (ec)
      return self.complete(error_code{}, stream{get_executor(), nullptr});

    if (jar)
    {
      auto cookie_itr = str->headers().find(http::field::set_cookie);
      if (cookie_itr != str->headers().end())
      {
        auto f = requests::parse_set_cookie_field(cookie_itr->value());
        if (f)
          jar->set(*f, this_->host());
        else
        {
          ec = f.error();
          return self.complete(ec, *std::move(str));
        }
      }
    }
  }
  // coro is complete
  if (is_complete())
      return self.complete(ec, *std::move(str));
}


void connection_impl::connect(endpoint_type ep, system::error_code & ec)
{
  next_layer_.next_layer().connect(endpoint_ = ep, ec);
  if (use_ssl_ && !ec)
    next_layer_.handshake(asio::ssl::stream_base::client, ec);
}


void connection_impl::close(system::error_code & ec)
{
  if (use_ssl_)
    next_layer_.shutdown(ec);

  if (next_layer_.next_layer().is_open())
    next_layer_.next_layer().close(ec);
}

std::size_t connection_impl::do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser, ec);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser, ec);

}

void connection_impl::do_async_read_some_(beast::http::basic_parser<false> & parser, asio::any_completion_handler<void(system::error_code, std::size_t)> tk)
{
  if (use_ssl_)
    beast::http::async_read_some(next_layer_, buffer_, parser, std::move(tk));
  else
    beast::http::async_read_some(next_layer_.next_layer(), buffer_, parser, std::move(tk));
}

void connection_impl::do_async_close_(asio::any_completion_handler<void(system::error_code)> tk)
{
  async_close_impl(std::move(tk), this);
}

struct connection_impl::async_connect_op : asio::coroutine
{
  constexpr static const char * op_name = "connection_impl::async_connect_op";

  connection_impl * this_;
  endpoint_type ep;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD this_->next_layer_.next_layer().async_connect(this_->endpoint_ = ep, std::move(self));
      if (this_->use_ssl_)
      {
        BOOST_REQUESTS_YIELD this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
      }
    }
    // coro is complete
    if (is_complete())
      self.complete(ec);
  }
};

struct connection_impl::async_close_op : asio::coroutine
{
  constexpr static const char * op_name =  "connection_impl::async_close_op";
  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code)>;

  connection_impl * this_;
  endpoint_type ep;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      if (!ec && this_->use_ssl_)
      {
        BOOST_REQUESTS_YIELD this_->next_layer_.async_shutdown(std::move(self));
      }
      if (this_->next_layer_.next_layer().is_open())
        this_->next_layer_.next_layer().close(ec);
    }
    // coro is complete
    if (is_complete())
      self.complete(ec);
  }
};

void connection_impl::do_close_(system::error_code & ec)
{
  if (ec)
    return;

  if (use_ssl_)
    next_layer_.shutdown(ec);

  if (next_layer_.next_layer().is_open())
    next_layer_.next_layer().close(ec);
}

void connection_impl::async_connect_impl(asio::any_completion_handler<void(error_code)> handler,
                                         connection_impl * this_, endpoint_type ep)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
      async_connect_op{{}, this_, ep}, handler, this_->get_executor());
}

void connection_impl::async_close_impl(asio::any_completion_handler<void(error_code)> handler,
                                       connection_impl * this_)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
      async_close_op{{}, this_}, handler, this_->get_executor());
}

void connection_impl::async_ropen_impl(asio::any_completion_handler<void(error_code, stream)> handler,
                                       connection_impl * this_, http::verb method,
                                       urls::pct_string_view path, http::fields & headers,
                                       source & src, cookie_jar * jar)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, stream)>, void(error_code, stream)>(
      async_ropen_op{this_->shared_from_this(), method, path, headers, src, jar},
      handler, this_->get_executor());
}

auto connection_impl::upgrade(urls::pct_string_view path,
                              http::fields & headers,
                              cookie_jar * jar) -> websocket
{
  system::error_code ec;
  auto res = upgrade(path, headers, jar, ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}

websocket connection_impl::upgrade(
    urls::pct_string_view path,
    http::fields & headers,
    cookie_jar * jar,
    system::error_code & ec)
{
  const auto is_secure = use_ssl_;
  if (jar)
  {
    auto cc = jar->get(host(), is_secure, path);
    if (!cc.empty())
      headers.set(http::field::cookie, cc);
    else
      headers.erase(http::field::cookie);
  }
  else
    headers.erase(http::field::cookie);

  headers.set(http::field::host, host_);
  if (headers.count(http::field::user_agent) > 0)
    headers.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

  if (!is_open())
  {
    next_layer_.next_layer().connect(endpoint_, ec);
    if (use_ssl_ && !ec)
      next_layer_.handshake(asio::ssl::stream_base::client, ec);

    if (ec)
      return websocket{detail::optional_ssl_stream<>{get_executor()}};
  }

  websocket ws{detail::optional_ssl_stream<>(std::move(next_layer_), use_ssl_)};

  ws.set_option(
      beast::websocket::stream_base::decorator(
          [&](beast::websocket::request_type & req)
          {
            for (auto & hd : headers)
              if (hd.name() == http::field::unknown)
                req.set(hd.name_string(), hd.value());
              else
                req.set(hd.name(), hd.value());
          }));

  beast::websocket::response_type res;
  ws.handshake(res, host_, core::string_view(path), ec);

  if (jar)
  {
    auto cookie_itr = res.find(http::field::set_cookie);
    if (cookie_itr != res.end())
    {
      auto f = requests::parse_set_cookie_field(cookie_itr->value());
      if (f)
        jar->set(*f, host_);
      else
      {
        ec = f.error();
        return ws;
      }
    }
  }
  return ws;
}


struct connection_impl::async_upgrade_op
    : boost::asio::coroutine
{
  constexpr static const char * op_name = "connection_impl::async_upgrade_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  std::shared_ptr<connection_impl> this_;

  std::shared_ptr<std::pair<websocket, beast::websocket::response_type>> websocket_;

  urls::pct_string_view path;
  http::fields & headers;
  cookie_jar * jar{nullptr};

  async_upgrade_op(std::shared_ptr<connection_impl> this_,
                   urls::pct_string_view path,
                   http::fields & headers,
                   cookie_jar * jar)
      : this_(std::move(this_)), path(path), headers(headers), jar(jar)
  {
  }

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t res_ = 0u);
};


template<typename Self>
void connection_impl::async_upgrade_op::operator()(
            Self && self,
            system::error_code ec, std::size_t res_)
{
  BOOST_ASIO_HANDLER_LOCATION((__FILE__, __LINE__, op_name));

  BOOST_ASIO_CORO_REENTER(this)
  {
    if (jar)
    {

      auto cc = jar->get(this_->host(), this_->use_ssl_, path);
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
      else
        headers.erase(http::field::cookie);
    }
    else
      headers.erase(http::field::cookie);

    headers.set(http::field::host, this_->host_);
    if (headers.count(http::field::user_agent) > 0)
      headers.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    if (!this_->is_open())
    {
      BOOST_REQUESTS_YIELD this_->next_layer_.next_layer().async_connect(this_->endpoint_, std::move(self));

      if (!ec && this_->use_ssl_)
      {
        BOOST_REQUESTS_YIELD this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
      }
      if (ec)
        break;
    }

    websocket_ = std::allocate_shared<std::pair<websocket, beast::websocket::response_type>>(
                                            asio::get_associated_allocator(self),
                                            detail::optional_ssl_stream<>(std::move(this_->next_layer_), this_->use_ssl_),
                                              beast::websocket::response_type{});
    websocket_->first.set_option(
        beast::websocket::stream_base::decorator(
            [&headers = headers](beast::websocket::request_type & req)
            {
              for (auto & hd : headers)
                if (hd.name() == beast::http::field::unknown)
                  req.set(hd.name_string(), hd.value());
                else
                  req.set(hd.name(), hd.value());
            }));
    BOOST_REQUESTS_YIELD websocket_->first.async_handshake(websocket_->second, this_->host_, core::string_view(path), std::move(self));
    if (ec)
      break;

    if (jar)
    {
      auto cookie_itr = websocket_->second.find(http::field::set_cookie);
      if (cookie_itr != websocket_->second.end())
      {
        auto f = requests::parse_set_cookie_field(cookie_itr->value());
        if (f)
          jar->set(*f, this_->host());
        else
        {
          ec = f.error();
          break;
        }
      }
    }
  }
  // coro is complete
  if (is_complete())
  {
    if (!websocket_)
      return self.complete(ec, websocket{detail::optional_ssl_stream<>{get_executor()}});
    else
    {
      auto tmp = std::move(websocket_->first);
      websocket_ = nullptr;
      return self.complete(ec, std::move(tmp));
    }
  }
}


void connection_impl::async_upgrade_impl(asio::any_completion_handler<void(error_code, websocket)> handler,
                                         connection_impl * this_, urls::pct_string_view path,
                                         http::fields & headers, cookie_jar * jar)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, websocket)>, void(error_code, websocket)>(
      async_upgrade_op{this_->shared_from_this(), path, headers, jar},
      handler, this_->get_executor());
}


}
}
}

#include <boost/requests/detail/undefine.hpp>
