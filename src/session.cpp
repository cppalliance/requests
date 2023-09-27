//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <boost/requests/detail/define.hpp>
#include <boost/requests/session.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/beast/version.hpp>

namespace boost {
namespace requests {


auto session::ropen(
    beast::http::verb method,
    urls::url_view url,
    http::fields & headers,
    source & src,
    system::error_code & ec) -> stream
{
  /*auto opts = options_;

  response_base::history_type history{headers.get_allocator()};

  auto do_ropen =
      [&](http::fields & hd, urls::pct_string_view target, request_options opts) -> stream
  {
    auto p = get_pool(url, ec);
    if (ec)
      return stream{get_executor(), nullptr};

    return p->ropen(method, target, hd, src, &jar_, ec);
  };

  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                         || (url.scheme_id() == urls::scheme::wss);

  auto host = url.encoded_host();

  if (!is_secure && opts.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  {
    auto cc = jar_.get(host, is_secure, url.encoded_path());
    if (!cc.empty())
      headers.set(http::field::cookie, cc);
  }


  auto str = do_ropen(headers, url.encoded_target(), opts);

  if (!ec) // all good
    return str;

  if (opts.max_redirects == str.history().size())
    return str;

  urls::url url_cache;
  while (ec == error::forbidden_redirect)
  {
    ec.clear();
    if (str.history().empty())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break;
    }
    opts.max_redirects -= str.history().size();
    const auto & last = str.history().back();
    auto rc = last.base().result();

    auto loc_itr = last.base().find(http::field::location);

    if (opts.max_redirects == 0)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
      break ;
    }

    if (((rc != http::status::moved_permanently)
         && (rc != http::status::found)
         && (rc != http::status::temporary_redirect)
         && (rc != http::status::permanent_redirect))
        || (loc_itr == last.base().end()))
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break;
    }

    const auto nurl = interpret_location(url.encoded_resource(), loc_itr->value());
    if (nurl.has_error())
    {
      ec = nurl.error();
      break;
    }

    if (!should_redirect(options_.redirect, url, *nurl))
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
      break ;
    }

    if (nurl->has_authority())
      url = *nurl;
    else
    {
      url_cache = url;
      url_cache.set_encoded_path(nurl->encoded_path());
      url = url_cache;
    }


    {
      auto cc = jar_.get(host, is_secure, url.encoded_path());
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
    }
    history.insert(history.end(),
                   std::make_move_iterator(std::move(str).history().begin()),
                   std::make_move_iterator(std::move(str).history().end()));

    str = do_ropen(headers, url.encoded_target(), opts);
  }
  str.prepend_history(std::move(history));
  return str;*/
}

auto session::make_request_(http::fields fields) -> requests::request_parameters {
  return requests::request_parameters{
      std::move(fields),
      options_,
      &jar_
  };
}

urls::url session::normalize_(urls::url_view in)
{
  urls::url res;
  if (!in.has_scheme() || in.scheme() == "https")
  {
    res.set_scheme("https");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else if (in.scheme() == "http")
  {
    res.set_scheme("http");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else
    return in; // just do some invalid shit
}


auto
session::get_pool(urls::url_view url, error_code & ec) -> std::shared_ptr<connection_pool>
{
  auto host_key = normalize_(url);

  std::lock_guard<std::mutex> lock(mutex_);
  if (ec)
    return std::shared_ptr<connection_pool>();

  auto itr = pools_.find(host_key);
  if (itr != pools_.end())
    return itr->second;
  else
  {
    auto p = std::make_shared<connection_pool>(get_executor(), sslctx_);
    p->lookup(host_key, ec);
    if (!ec)
    {
      pools_.emplace(host_key, p);
      return p;
    }
  }
  return nullptr;
}

struct session::async_get_pool_op : asio::coroutine
{
  constexpr static const char * op_name = "session::async_get_pool_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session *this_;
  urls::url_view url;
  const bool is_https;

  async_get_pool_op(session *this_, urls::url_view url)
      : this_(this_), url(url),
        is_https((url.scheme_id() == urls::scheme::https) || (url.scheme_id() == urls::scheme::wss))
  {}

  std::shared_ptr<connection_pool> p;

  template<typename Self>
  void operator()(Self && self, error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {

      {
        std::lock_guard<std::mutex> lock{this_->mutex_};
        auto itr = this_->pools_.find(url);
        if (itr != this_->pools_.end())
          return self.complete(ec, std::move(itr->second));
      }
      p = std::make_shared<connection_pool>(this_->get_executor(), this_->sslctx_);
      BOOST_REQUESTS_YIELD p->async_lookup(url, std::move(self));
      if (!ec)
      {
        std::lock_guard<std::mutex> lock{this_->mutex_};
        auto itr = this_->pools_.find(url);
        if (itr == this_->pools_.end())
          this_->pools_.emplace(url, p);
        else
          p = itr->second;
        return self.complete(ec, p);
      }
    }
  }

};


struct session::async_ropen_op : asio::coroutine
{
  constexpr static const char * op_name = "session::async_ropen_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session * this_;

  http::verb method;
  urls::url url;
  struct request_options opts;

  bool is_secure = (url.scheme_id() == urls::scheme::https)
                || (url.scheme_id() == urls::scheme::wss);

  http::fields & headers;
  source & src;

  async_ropen_op(session * this_,
                 http::verb method,
                 urls::url_view path,
                 source & src,
                 http::fields & headers)
      : this_(this_), method(method), url(path), opts(this_->options_), headers(headers), src(src)
  {
  }

  using completion_signature_type = void(system::error_code, stream);
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream>);

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {},
                  variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream> s  = variant2::monostate{});
};


template<typename Self>
void session::async_ropen_op::operator()(
    Self && self, system::error_code  ec,
    variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream> s)
{
    BOOST_ASIO_CORO_REENTER(this)
    {
      headers.set(beast::http::field::host, url.encoded_host_and_port());
      headers.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

      if (!is_secure && this_->options_.enforce_tls)
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
        BOOST_REQUESTS_YIELD {
            auto exec = asio::get_associated_immediate_executor(self, this_->get_executor());
            asio::dispatch(exec, asio::append(std::move(self), ec));
        };
      }

      {
        auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path());
        if (!cc.empty())
          headers.set(http::field::cookie, cc);
      }

      BOOST_REQUESTS_YIELD this_->async_get_pool(url, std::move(self));
      BOOST_REQUESTS_YIELD variant2::get<1>(s)->async_ropen(method, url.encoded_resource(),
                                             headers, src, opts, &this_->jar_, std::move(self));

      return self.complete(ec, std::move(variant2::get<2>(s)));

    }
  if (is_complete())
    self.complete(ec, stream{this_->get_executor(), nullptr});
}

BOOST_REQUESTS_DECL
void session::async_ropen_impl(
    asio::any_completion_handler<void (boost::system::error_code, stream)> handler,
    session * sess, http::verb method, urls::url_view path, source * src, http::fields * headers)
{
  asio::async_compose<
            asio::any_completion_handler<void (boost::system::error_code, stream)>,
            void (boost::system::error_code, stream)>(
            async_ropen_op{
                sess, method, path, *src, *headers}, handler, sess->get_executor()
            );
}


void session::async_get_pool_impl(
    asio::any_completion_handler<void (boost::system::error_code, std::shared_ptr<connection_pool>)> handler,
    session * sess, urls::url_view url)
{
  asio::async_compose<
      asio::any_completion_handler<void (boost::system::error_code, std::shared_ptr<connection_pool>)>,
      void (boost::system::error_code, std::shared_ptr<connection_pool>)>(
        async_get_pool_op{sess, url},
        handler,
        sess->get_executor()
      );
}

}
}

#include <boost/requests/detail/undefine.hpp>

