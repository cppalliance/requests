//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_IPP
#define BOOST_REQUESTS_IMPL_CONNECTION_IPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/asio/yield.hpp>

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

}


auto connection::ropen(beast::http::verb method,
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


auto connection::ropen(beast::http::verb method,
                       urls::pct_string_view path,
                       http::fields & headers,
                       source & src,
                       request_options opt,
                       cookie_jar * jar,
                       system::error_code & ec) -> stream
{
  const auto is_secure = use_ssl_;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  detail::tracker t{this->ongoing_requests_};
  lock_type read_lock;
  if (jar)
  {
    detail::monotonic_token mv;
    auto cc = jar->get(host(), is_secure, path, mv);
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
        return stream{get_executor(), this};
      lock_type wlock{write_mtx_, std::adopt_lock};
      boost::optional<lock_type> alock;

      // disconnect first
      if (!is_open() && keep_alive_set_.timeout < std::chrono::system_clock::now()) {
        read_mtx_.lock(ec);
        if (ec)
          return stream{get_executor(), this};

        alock.emplace(read_mtx_, std::adopt_lock);
        // if the close goes wrong - so what, unless it's still open
        if (use_ssl_)
          next_layer_.shutdown(ec);
        if (!ec)
          next_layer_.next_layer().close(ec);
        ec.clear();
      }

      if (!is_open())
      {
      retry:
        if (!alock)
        {
          read_mtx_.lock(ec);
          if (ec)
            return stream{get_executor(), this};

          alock.emplace(read_mtx_, std::adopt_lock);
        }

        next_layer_.next_layer().connect(endpoint_, ec);
        if (use_ssl_ && !ec)
          next_layer_.handshake(asio::ssl::stream_base::client, ec);
        if (ec)
          return stream{get_executor(), this};
      }

      alock.reset();
      if (use_ssl_)
        write_request(next_layer_,              method, path, headers, src);
      else
        write_request(next_layer_.next_layer(), method, path, headers, src);

      if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
        goto retry ;
      else  if (ec)
        return stream{get_executor(), this};

      // release after acquire!
      read_mtx_.lock(ec);

      if (ec)
        return stream{get_executor(), this};

      read_lock = {read_mtx_, std::adopt_lock};
    }
    // write end

    if (ec)
      return stream{get_executor(), this};

    stream str{get_executor(), this};
    str.parser_ = detail::make_pmr<http::response_parser<http::buffer_body>>(headers.get_allocator().resource(),
                                                                             http::response_header{http::fields(headers.get_allocator())});

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

    if ((opt.redirect < redirect_mode::endpoint)
        || ((rc != http::status::moved_permanently)
            && (rc != http::status::found)
            && (rc != http::status::temporary_redirect)
            && (rc != http::status::permanent_redirect)))
    {
      // GO
      str.t_ = std::move(t);
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
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
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
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      break ;
    }

    if (--opt.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      break ;
    }

    path = url->encoded_resource();
    if (jar)
    {
      detail::monotonic_token mv;
      auto cc = jar->get(host(), is_secure, url->encoded_path(), mv);

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


void connection::set_host(core::string_view sv, system::error_code & ec)
{
  next_layer_.set_verify_callback(asio::ssl::host_name_verification(host_ = sv), ec);
}


BOOST_REQUESTS_DECL
auto connection::async_ropen_op::resume(
            requests::detail::co_token_t<step_signature_type> self,
            system::error_code & ec, std::size_t res_) -> stream
{
  reenter(this)
  {
    if (ec_)
    {
      yield asio::post(this_->get_executor(), std::move(self));
      ec = ec_;
    }

    if (jar)
    {
      detail::monotonic_token mv;
      auto cc = jar->get(this_->host(), this_->use_ssl_, path, mv);
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
      await_lock(this_->write_mtx_, lock);

      // disconnect first
      if (!this_->is_open() && this_->keep_alive_set_.timeout < std::chrono::system_clock::now())
      {
        await_lock(this_->read_mtx_, alock);
        // if the close goes wrong - so what, unless it's still open
        if (!ec && this_->use_ssl_)
        {
          yield this_->next_layer_.async_shutdown(std::move(self));
        }
        if (!ec)
          this_->next_layer_.next_layer().close(ec);
        ec.clear();
      }

      if (!this_->is_open())
      {
      retry:
        if (!alock)
        {
          await_lock(this_->read_mtx_, alock);
        }
        yield this_->next_layer_.next_layer().async_connect(this_->endpoint_, std::move(self));
        if (!ec && this_->use_ssl_)
        {
          yield this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
        }
        if (ec)
          break;
      }

      alock.reset();
      if (this_->use_ssl_)
      {
        yield async_write_request(this_->next_layer_, method, path, headers, src, std::move(self));
      }
      else
      {
        yield async_write_request(this_->next_layer_.next_layer(), method, path, headers, src, std::move(self));
      }

      if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
        goto retry ;
      else if (ec)
        break;

      // release after acquire!
      await_lock(this_->read_mtx_, lock);
      // END OF write impl

      str.emplace(this_->get_executor(), this_); // , req.get_allocator().resource()
      str->parser_ = detail::make_pmr<http::response_parser<http::buffer_body>>(headers.get_allocator().resource(),
                                                                                http::response_header{http::fields(headers.get_allocator())});

      if (this_->use_ssl_)
      {
        yield beast::http::async_read_header(this_->next_layer_, this_->buffer_, *str->parser_, std::move(self));
      }
      else
      {
        yield beast::http::async_read_header(this_->next_layer_.next_layer(), this_->buffer_, *str->parser_, std::move(self));
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
              return *std::move(str);
            }
          }
        }

        auto rc = str->parser_->get().base().result();
        if ((opts.redirect < redirect_mode::endpoint) ||
            ((rc != http::status::moved_permanently) && (rc != http::status::found) &&
             (rc != http::status::temporary_redirect) && (rc != http::status::permanent_redirect)))
        {
          // GO
          str->t_ = std::move(t);
          str->lock_ = std::move(lock);
          str->history_ = std::move(history);
          return *std::move(str);
        }
      }

      if (method != http::verb::head)
      {
        yield str->async_read(buf, std::move(self));
      }
      if (ec)
        break;
      history.emplace_back(std::move( str->parser_->get().base()), std::move(buf));
      lock = {};
      str.reset();

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
      if (jar)
      {
        detail::monotonic_token mv;
        auto cc = jar->get(this_->host(), this_->use_ssl_, url->encoded_path(), mv);
        if (!cc.empty())
          headers.set(http::field::cookie, cc);
        else
          headers.erase(http::field::cookie);
      }
      else
        headers.erase(http::field::cookie);

    }

    stream str_{this_->get_executor(), this_};
    str_.history_ = std::move(history);
    return str_;

  }
  return stream{this_->get_executor(), this_};

}


void connection::connect(endpoint_type ep, system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;

  next_layer_.next_layer().connect(endpoint_ = ep, ec);

  if (use_ssl_ && !ec)
    next_layer_.handshake(asio::ssl::stream_base::client, ec);
}


void connection::close(system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;

  if (use_ssl_)
    next_layer_.shutdown(ec);

  if (ec)
    return ;

  next_layer_.next_layer().close(ec);
}


std::size_t connection::do_read_some_(beast::http::basic_parser<false> & parser)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser);
}

std::size_t connection::do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser, ec);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser, ec);
}

void connection::do_async_read_some_(beast::http::basic_parser<false> & parser, detail::co_token_t<void(system::error_code, std::size_t)> tk)
{
  if (use_ssl_)
    beast::http::async_read_some(next_layer_, buffer_, parser, std::move(tk));
  else
    beast::http::async_read_some(next_layer_.next_layer(), buffer_, parser, std::move(tk));
}

void connection::do_async_close_(detail::co_token_t<void(system::error_code)> tk)
{
  async_close(std::move(tk));
}

void connection::async_connect_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                          system::error_code & ec)
{
  reenter(this)
  {
    await_lock(this_->write_mtx_, write_lock);
    await_lock(this_->read_mtx_,  read_lock);
    yield this_->next_layer_.next_layer().async_connect(this_->endpoint_ = ep, std::move(self));
    if (!ec && this_->use_ssl_)
    {
      yield this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
    }
  }
}

void connection::async_close_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                          system::error_code & ec)
{
  reenter(this)
  {
    await_lock(this_->write_mtx_, write_lock);
    await_lock(this_->read_mtx_,  read_lock);
    if (!ec && this_->use_ssl_)
    {
      yield this_->next_layer_.async_shutdown(std::move(self));
    }
    if (!ec)
      this_->next_layer_.next_layer().close(ec);
  }
}

}
}

#include <boost/requests/detail/undefine.hpp>
#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_REQUESTS_CONNECTION_IPP
