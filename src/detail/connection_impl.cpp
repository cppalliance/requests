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


bool check_endpoint(
    urls::url_view path,
    const asio::ip::tcp::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec)
{
  if ((path.has_port() && (get_port(path) != ep.port()))
      && (path.has_authority() && (path.encoded_host() != host))
      && (path.has_scheme() && (path.host() != (has_ssl ? "https" : "http"))))
    BOOST_REQUESTS_ASSIGN_EC(ec, error::wrong_host)

  return !ec;
}

bool check_endpoint(
    urls::url_view path,
    const asio::local::stream_protocol::endpoint & ep,
    core::string_view host,
    bool,
    system::error_code & ec)
{
  if (path.has_port()
      && (path.has_authority() && (path.host() != host))
      && (path.has_scheme() && (path.host() != "unix")))
    BOOST_REQUESTS_ASSIGN_EC(ec, error::wrong_host)

  return !ec;
}

bool check_endpoint(
    urls::url_view path,
    const asio::generic::stream_protocol::endpoint  & ep,
    core::string_view host,
    bool has_ssl,
    system::error_code & ec)
{
  if (ep.protocol() == asio::local::stream_protocol())
  {
    asio::local::stream_protocol::endpoint cmp;
    cmp.resize(ep.size());
    std::memcpy(cmp.data(),
                ep.data(),
                ep.size());
    return check_endpoint(path, cmp, host, has_ssl, ec);
  }
  else if (ep.protocol() == asio::ip::tcp::v4()
           || ep.protocol() == asio::ip::tcp::v6())
  {
    asio::ip::tcp::endpoint cmp;
    cmp.resize(ep.size());
    std::memcpy(cmp.data(),
                ep.data(),
                ep.size());
    return check_endpoint(path, cmp, host, has_ssl, ec);
  }
  else
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::no_protocol_option);
    return false;
  }
}


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
                            urls::url_view path,
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
                            urls::url_view path,
                            http::fields & headers,
                            source & src,
                            cookie_jar * jar,
                            system::error_code & ec) -> stream
{
  detail::check_endpoint(path, endpoint(), host(), use_ssl_, ec);
  if (ec)
    return stream{get_executor(), nullptr};

  return ropen(method, path.encoded_resource(), headers, src, jar, ec);
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
  if (headers.count(http::field::user_agent) > 0)
    headers.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

  detail::lock_guard lock{write_mtx_};
  // write impl
  {
    if (ec)
      return stream{get_executor(), nullptr};
    if (!is_open())
    {
      if (ec)
        return stream{get_executor(), nullptr};

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
  lock = read_mtx_;
  // write end

  if (ec)
    return stream{get_executor(), nullptr};

  stream str{get_executor(), this};
  str.parser_ = std::make_unique<http::response_parser<http::buffer_body>>(http::response_header{http::fields(headers.get_allocator())});
  str.parser_->body_limit(boost::none);

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
  str.lock_ = std::move(lock);
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

  boost::intrusive_ptr<connection_impl> this_;

  optional<stream> str;
  beast::http::verb method;
  urls::pct_string_view path;
  http::fields & headers;
  source & src;
  cookie_jar * jar{nullptr};
  system::error_code ec_;
  lock_guard lock_;

  async_ropen_op(boost::intrusive_ptr<connection_impl> this_,
                 beast::http::verb method,
                 urls::pct_string_view path,
                 http::fields & headers,
                 source & src,
                 cookie_jar * jar)
      : this_(std::move(this_)), method(method), path(path), headers(headers), src(src)
      , jar(jar)
  {
  }

  async_ropen_op(boost::intrusive_ptr<connection_impl> this_,
                 beast::http::verb method,
                 urls::url_view path,
                 http::fields & headers,
                 source & src,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path.encoded_resource()),
        headers(headers), src(src), jar(jar)
  {
    detail::check_endpoint(path, this_->endpoint(), this_->host(), this_->use_ssl_, ec_);
  }

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t res_ = 0u);
};


template<typename Self>
void connection_impl::async_ropen_op::operator()(
            Self && self,
            system::error_code ec, std::size_t res_)
{
  BOOST_ASIO_HANDLER_LOCATION((__FILE__, __LINE__, op_name));

  BOOST_ASIO_CORO_REENTER(this)
  {
    if (ec_)
    {
      BOOST_REQUESTS_YIELD asio::post(this_->get_executor(), std::move(self));
      ec = ec_;
      break;
    }

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

    if (!this_->write_mtx_.try_lock())
    {
      BOOST_REQUESTS_YIELD this_->write_mtx_.async_lock(std::move(self));
      if (ec)
        return self.complete(ec, *std::move(str));
      lock_ = {this_->write_mtx_, std::adopt_lock};
    }

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
    str->parser_ = std::make_unique<http::response_parser<http::buffer_body>>(http::response_header{http::fields(headers.get_allocator())});
    str->parser_->body_limit(boost::none);

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
    {
      lock_ = {};
      return self.complete(error_code{}, stream{get_executor(), nullptr});
    }

    if (!this_->read_mtx_.try_lock())
    {
      BOOST_REQUESTS_YIELD this_->read_mtx_.async_lock(std::move(self));
      if (ec)
        return self.complete(ec, *std::move(str));
      lock_ = {this_->read_mtx_, std::adopt_lock};
    }


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
  {
      lock_ = {};
      return self.complete(ec, *std::move(str));
  }
}


void connection_impl::connect(endpoint_type ep, system::error_code & ec)
{
  detail::lock_guard lw{write_mtx_}, lr{read_mtx_};
  next_layer_.next_layer().connect(endpoint_ = ep, ec);

  if (use_ssl_ && !ec)
    next_layer_.handshake(asio::ssl::stream_base::client, ec);
}


void connection_impl::close(system::error_code & ec)
{
  detail::lock_guard lw{write_mtx_}, lr{read_mtx_};
  if (use_ssl_)
    next_layer_.shutdown(ec);

  if (next_layer_.next_layer().is_open())
    next_layer_.next_layer().close(ec);
}


std::size_t connection_impl::do_read_some_(beast::http::basic_parser<false> & parser)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser);
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

  detail::lock_guard write_lock, read_lock;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_AWAIT_LOCK(this_->write_mtx_, write_lock);
      BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_, read_lock);
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

  detail::lock_guard write_lock, read_lock;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_AWAIT_LOCK(this_->write_mtx_, write_lock);
      BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_, read_lock);
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

void connection_impl::return_to_pool_()
{
  if (auto ptr = borrowed_from_.load())
    ptr->return_connection_(this);
}

void connection_impl::remove_from_pool_()
{
  if (auto ptr = borrowed_from_.load())
    ptr->drop_connection_(this);
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
      async_ropen_op{this_, method, path, headers, src, jar},
      handler, this_->get_executor());
}

void connection_impl::async_ropen_impl_url(asio::any_completion_handler<void(error_code, stream)> handler,
                                           connection_impl * this_, http::verb method,
                                           urls::url_view url, http::fields & headers,
                                           source & src, cookie_jar * jar)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, stream)>, void(error_code, stream)>(
      async_ropen_op{this_, method, url, headers, src, jar},
      handler, this_->get_executor());
}

}
}
}

#include <boost/requests/detail/undefine.hpp>
