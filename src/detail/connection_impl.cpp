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
                       request_options opt,
                       cookie_jar * jar) -> stream
{
  system::error_code ec;
  auto res = ropen(method, path, headers, src, std::move(opt), jar, ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}


auto connection_impl::ropen(beast::http::verb method,
                       urls::pct_string_view path,
                       http::fields & headers,
                       source & src,
                       request_options opt,
                       cookie_jar * jar,
                       system::error_code & ec) -> stream
{
  const auto is_secure = use_ssl_;
  using lock_type = detail::lock_guard;
  lock_type read_lock;
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

  response_base::history_type history;

  while (!ec)
  {
    // write impl
    {
      write_mtx_.lock(ec);
      if (ec)
        return stream{get_executor(), nullptr};
      lock_type wlock{write_mtx_, std::adopt_lock};
      boost::optional<lock_type> alock;

      if (!is_open())
      {
      retry:
        read_mtx_.lock(ec);
        if (ec)
          return stream{get_executor(), nullptr};

        alock.emplace(read_mtx_, std::adopt_lock);

        next_layer_.next_layer().connect(endpoint_, ec);
        if (use_ssl_ && !ec)
          next_layer_.handshake(asio::ssl::stream_base::client, ec);
        if (ec)
          return stream{get_executor(), nullptr};
      }

      alock.reset();

      if (use_ssl_)
        write_request(next_layer_,              method, path, headers, src);
      else
        write_request(next_layer_.next_layer(), method, path, headers, src);

      if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
        goto retry ;
      else if (ec)
        return stream{get_executor(), nullptr};

      // release after acquire!
      read_mtx_.lock(ec);

      if (ec)
        return stream{get_executor(), nullptr};

      read_lock = {read_mtx_, std::adopt_lock};
    }
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
      break;

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

    auto rc = str.parser_->get().base().result();

    if ((opt.redirect == redirect_mode::none)
        || ((rc != http::status::moved_permanently)
         && (rc != http::status::found)
         && (rc != http::status::temporary_redirect)
         && (rc != http::status::permanent_redirect)))
    {
      // GO
      str.lock_ = std::move(read_lock);
      str.history_ = std::move(history);
      return str;
    }
    response_base::buffer_type buf{headers.get_allocator()};
    if (method != http::verb::head)
      str.read(buf, ec);
    if (ec)
      break;

    history.emplace_back(std::move( str.parser_->get().base()), std::move(buf));

    auto & res = history.back().base();

    // read the body to put into history
    auto loc_itr = res.find(http::field::location);
    if (loc_itr == res.end())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break ;
    }

    const auto url = interpret_location(path, loc_itr->value());
    if (url.has_error())
    {
      ec = url.error();
      break;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority() &&
        host_ == url->encoded_host() &&
        !same_endpoint_on_host(*url, endpoint()))
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
      break ;
    }

    if (--opt.max_redirects == 0)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
      break ;
    }

    path = url->encoded_resource();
    if (!url->encoded_host_and_port().empty())
      headers.set(http::field::host, url->encoded_host_and_port());

    if (jar && headers.count(http::field::host) > 0u)
    {
      auto cc = jar->get(headers[http::field::host], is_secure, url->encoded_path());
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
      else
        headers.erase(http::field::cookie);
    }
    else
      headers.erase(http::field::cookie);

    read_lock = {};
  }

  stream str{get_executor(), this};
  str.history_ = std::move(history);
  return str;
}


void connection_impl::set_host(core::string_view sv, system::error_code & ec)
{
  next_layer_.set_verify_callback(asio::ssl::host_name_verification(host_ = sv), ec);
}


struct connection_impl::async_ropen_op
    : boost::asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  using lock_type = detail::lock_guard;

  boost::intrusive_ptr<connection_impl> this_;

  optional<stream> str;
  beast::http::verb method;
  urls::pct_string_view path;
  http::fields & headers;
  source & src;
  request_options opts;
  cookie_jar * jar{nullptr};
  system::error_code ec_;

  struct state_t
  {
    response_base::buffer_type buf;

    lock_type lock;
    boost::optional<lock_type> alock;

    response_base::history_type history;
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(boost::system::error_code, stream)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  async_ropen_op(allocator_type alloc,
                 boost::intrusive_ptr<connection_impl> this_,
                 beast::http::verb method,
                 urls::pct_string_view path,
                 http::fields & headers,
                 source & src,
                 request_options opts,
                 cookie_jar * jar)
      : this_(std::move(this_)), method(method), path(path), headers(headers), src(src), opts(std::move(opts))
      , jar(jar), state(allocate_unique<state_t>(alloc))
  {
  }

  async_ropen_op(allocator_type alloc,
                 boost::intrusive_ptr<connection_impl> this_,
                 beast::http::verb method,
                 urls::url_view path,
                 http::fields & headers,
                 source & src,
                 request_options opt,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path.encoded_resource()),
        headers(headers), src(src), opts(std::move(opt)), jar(jar), state(allocate_unique<state_t>(alloc))
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
  auto st = state.get();
  if (!ec)
  BOOST_ASIO_CORO_REENTER(this)
  {
    if (ec_)
    {
      BOOST_ASIO_CORO_YIELD asio::post(this_->get_executor(), std::move(self));
      ec = ec_;
      break;
    }

    state = boost::allocate_unique<state_t>(asio::get_associated_allocator(self));

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


    while (!ec)
    {
      BOOST_REQUESTS_AWAIT_LOCK(this_->write_mtx_, st->lock);

      if (!this_->is_open())
      {
      retry:
        BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_, st->alock);
        BOOST_ASIO_CORO_YIELD this_->next_layer_.next_layer().async_connect(this_->endpoint_, std::move(self));
        if (!ec && this_->use_ssl_)
        {
          BOOST_ASIO_CORO_YIELD this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
        }
        if (ec)
          break;
      }

      st->alock.reset();
      if (this_->use_ssl_)
      {
        BOOST_ASIO_CORO_YIELD async_write_request(this_->next_layer_, method, path, headers, src, std::move(self));
      }
      else
      {
        BOOST_ASIO_CORO_YIELD async_write_request(this_->next_layer_.next_layer(), method, path, headers, src, std::move(self));
      }

      if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
        goto retry ;
      else if (ec)
        break;

      // release after acquire!
      BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_, st->lock);
      // END OF write impl

      str.emplace(this_->get_executor(), this_); // , req.get_allocator().resource()
      str->parser_ = std::make_unique<http::response_parser<http::buffer_body>>(http::response_header{http::fields(headers.get_allocator())});
      str->parser_->body_limit(boost::none);

      if (method == http::verb::head || method == http::verb::connect)
        str->parser_->skip(true);

      if (this_->use_ssl_)
      {
        BOOST_ASIO_CORO_YIELD beast::http::async_read_header(this_->next_layer_, this_->buffer_, *str->parser_, std::move(self));
      }
      else
      {
        BOOST_ASIO_CORO_YIELD beast::http::async_read_header(this_->next_layer_.next_layer(), this_->buffer_, *str->parser_, std::move(self));
      }

      if (ec)
        break;

      {
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

        auto rc = str->parser_->get().base().result();
        if ((opts.redirect < redirect_mode::endpoint) ||
            ((rc != http::status::moved_permanently) && (rc != http::status::found) &&
             (rc != http::status::temporary_redirect) && (rc != http::status::permanent_redirect)))
        {
          // GO
          str->lock_ = std::move(st->lock);
          str->history_ = std::move(st->history);
          return self.complete(ec, *std::move(str));
        }
      }

      if (method != http::verb::head)
      {
        BOOST_ASIO_CORO_YIELD str->async_read(st->buf, std::move(self));
      }
      if (ec)
        break;
      state->history.emplace_back(std::move(str->parser_->get().base()), std::move(st->buf));
      st->lock = {};
      str.reset();

      auto & res = state->history.back().base();

      // read the body to put into history
      auto loc_itr = res.find(http::field::location);
      if (loc_itr == res.end())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
        break ;
      }

      const auto url = interpret_location(path, loc_itr->value());
      if (url.has_error())
      {
        ec = url.error();
        break;
      }
      // we don't need to use should_redirect, bc we're on the same host.
      if (url->has_authority() &&
          this_->host_ == url->encoded_host() &&
          !same_endpoint_on_host(*url, this_->endpoint()))
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
        break ;
      }

      if (--opts.max_redirects == 0)
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
        break ;
      }

      path = url->encoded_resource();
      if (!url->encoded_host_and_port().empty())
        headers.set(http::field::host, url->encoded_host_and_port());

      if (jar && headers.count(http::field::host) > 0u)
      {
        auto cc = jar->get(headers[http::field::host], this_->use_ssl_, url->encoded_path());
        if (!cc.empty())
          headers.set(http::field::cookie, cc);
        else
          headers.erase(http::field::cookie);
      }
      else
        headers.erase(http::field::cookie);

    }

    stream str_{this_->get_executor(), nullptr};
    str_.history_ = std::move(st->history);
    return self.complete(ec, std::move(str_));

  }
  // coro is complete
  if (is_complete())
  {
      state.reset();
      self.complete(ec, stream{this_->get_executor(), nullptr});
  }
}


void connection_impl::connect(endpoint_type ep, system::error_code & ec)
{
  auto wlock = detail::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = detail::lock(read_mtx_, ec);
  if (ec)
    return;

  next_layer_.next_layer().connect(endpoint_ = ep, ec);

  if (use_ssl_ && !ec)
    next_layer_.handshake(asio::ssl::stream_base::client, ec);
}


void connection_impl::close(system::error_code & ec)
{
  auto wlock = detail::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = detail::lock(read_mtx_, ec);
  if (ec)
    return;

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
  using lock_type = detail::lock_guard;
  struct state_t
  {
    lock_type read_lock, write_lock;
  };
  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  connection_impl * this_;
  endpoint_type ep;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    auto st = state.get();
    if (!ec)
      BOOST_ASIO_CORO_REENTER(this)
      {
        BOOST_REQUESTS_AWAIT_LOCK(this_->write_mtx_, st->write_lock);
        BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_,  st->read_lock);
        if (this_->use_ssl_)
        {
          BOOST_ASIO_CORO_YIELD this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
        }
      }
    // coro is complete
    if (is_complete())
    {
      state.reset();
      self.complete(ec);
    }
  }
};

struct connection_impl::async_close_op : asio::coroutine
{
  using lock_type = detail::lock_guard;
  struct state_t
  {
    lock_type read_lock, write_lock;
  };
  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  connection_impl * this_;
  endpoint_type ep;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {})
  {
    auto st = state.get();
    if (!ec)
      BOOST_ASIO_CORO_REENTER(this)
      {
          BOOST_REQUESTS_AWAIT_LOCK(this_->write_mtx_, st->write_lock);
          BOOST_REQUESTS_AWAIT_LOCK(this_->read_mtx_,  st->read_lock);
          if (!ec && this_->use_ssl_)
          {
            BOOST_ASIO_CORO_YIELD this_->next_layer_.async_shutdown(std::move(self));
          }
          if (this_->next_layer_.next_layer().is_open())
            this_->next_layer_.next_layer().close(ec);
      }
    // coro is complete
    if (is_complete())
    {
      state.reset();
      self.complete(ec);
    }
  }
};

void connection_impl::do_close_(system::error_code & ec)
{
  auto wlock = detail::lock(write_mtx_, ec);
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

BOOST_REQUESTS_DECL
void connection_impl::async_connect_impl(asio::any_completion_handler<void(error_code)> handler,
                                         connection_impl * this_, endpoint_type ep)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
      async_connect_op{{},
                       allocate_unique<async_connect_op::state_t>(asio::get_associated_allocator(handler)),
                       this_, ep}, handler, this_->get_executor());
}

BOOST_REQUESTS_DECL
void connection_impl::async_close_impl(asio::any_completion_handler<void(error_code)> handler,
                                       connection_impl * this_)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
      async_close_op{{},
                     allocate_unique<async_close_op::state_t>(asio::get_associated_allocator(handler)),
                     this_}, handler, this_->get_executor());
}

BOOST_REQUESTS_DECL
void connection_impl::async_ropen_impl(asio::any_completion_handler<void(error_code, stream)> handler,
                             connection_impl * this_, http::verb method,
                             urls::pct_string_view path, http::fields & headers,
                             source & src, request_options opt, cookie_jar * jar)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, stream)>, void(error_code, stream)>(
      async_ropen_op{asio::get_associated_allocator(handler), this_, method, path, headers, src, std::move(opt), jar},
      handler, this_->get_executor());
}

}
}
}

#include <boost/requests/detail/undefine.hpp>
