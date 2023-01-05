//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_HPP
#define BOOST_REQUESTS_IMPL_SESSION_HPP

#include <boost/requests/session.hpp>
#include <boost/url/grammar/string_token.hpp>
#include <boost/asio/yield.hpp>

namespace boost {
namespace requests {


struct session::async_get_pool_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session *this_;
  struct impl_t
  {
    char buf[1024];
    container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};
    using alloc = container::pmr::polymorphic_allocator<char>;
    using str = std::basic_string<char, std::char_traits<char>, alloc>;
    urls::url host_key;
    const bool is_https;

    impl_t(urls::url_view url)
        : host_key{normalize_(url)}
        , is_https((url.scheme_id() == urls::scheme::https) || (url.scheme_id() == urls::scheme::wss))
    {
    }
  };

  urls::url_view url;
  std::shared_ptr<impl_t> impl;

  template<typename Self>
  void complete(Self && self,
                error_code ec,
                std::shared_ptr<connection_pool> pp,
                asem::lock_guard<detail::basic_mutex<executor_type>> &lock)
  {
    impl = {};
    lock = {};
    self.complete(ec, std::move(pp));
  }

  std::shared_ptr<connection_pool> p;

  template<typename Self>
  void operator()(Self && self, error_code ec = {},
                  asem::lock_guard<detail::basic_mutex<executor_type>> lock = {})
  {
    reenter(this)
    {
      impl = std::allocate_shared<impl_t>(self.get_allocator(), url);
      yield asem::async_lock(this_->mutex_, std::move(self));
      if (ec)
        return complete(std::move(self), ec, {}, lock);

      {
        auto itr = this_->pools_.find(impl->host_key);
        if (itr != this_->pools_.end())
          return complete(std::move(self), {}, itr->second, lock);
      }
      p = std::make_shared<connection_pool>(this_->get_executor(), this_->sslctx_);
      yield p->async_lookup(url, asio::append(std::move(self), std::move(lock)));
      if (!ec)
      {
        this_->pools_.emplace(impl->host_key, p);
        return complete(std::move(self), {}, std::move(p), lock);
      }

      return complete(std::move(self), ec, {}, lock);
    }
  }
};

template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
session::async_get_pool(urls::url_view url, CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, std::shared_ptr<connection_pool>)>(
      async_get_pool_op{{}, this, url},
      completion_token,
      mutex_
  );
}

template<typename Body>
auto session::ropen(
           urls::url_view url,
           http::request<Body>& req,
           system::error_code & ec) -> stream
{
  auto opts = options_;

  response_base::history_type history{req.get_allocator()};

  if (!url.encoded_target().empty() && req.target().empty())
    req.target(url.encoded_resource());

  auto do_ropen =
      [&](http::request<Body> & req, request_options opts) -> stream
      {
        auto p = get_pool(url, ec);
        if (ec)
          return stream{get_executor(), nullptr};

        return p->ropen( req, opts, &jar_, ec);
      };

  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);

  auto host = url.encoded_host();

  if (!is_secure && opts.enforce_tls)
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  {
    detail::monotonic_token mv;
    auto cc = jar_.get(host, is_secure, url.encoded_path(), mv);
    if (!cc.empty())
      req.set(http::field::cookie, cc);
  }

  req.prepare_payload();

  auto str = do_ropen(req, opts);

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

    const auto nurl = interpret_location(req.target(), loc_itr->value());
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

    req.base().target(url.encoded_resource());
    {
      detail::monotonic_token mv;
      auto cc = jar_.get(host, is_secure, url.encoded_path(), mv);
      if (!cc.empty())
        req.base().set(http::field::cookie, cc);
    }
    history.insert(history.end(),
                   std::make_move_iterator(std::move(str).history().begin()),
                   std::make_move_iterator(std::move(str).history().end()));
    str = do_ropen(req, opts);
  }
  str.prepend_history(std::move(history));
  return str;
}

template<typename RequestBody>
auto session::ropen(
    beast::http::verb method,
    urls::url_view url,
    RequestBody && body,
    http::fields fields,
    system::error_code & ec) -> stream
{
  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);

  auto host = url.encoded_host();

  if (!is_secure && options_.enforce_tls)
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;


  {
    const auto nm = body_traits::default_content_type(body);
    auto itr = fields.find(http::field::content_type);
    if (itr == fields.end() && !nm.empty())
        fields.set(http::field::content_type, nm);
  }

  auto bd = body_traits::make_body(std::forward<RequestBody>(body), ec);
  if (ec)
    return stream{get_executor(), nullptr};


  beast::http::request<body_type, http::fields> hreq{method, url.encoded_resource(), 11,
                                                     std::move(bd),
                                                     std::move(fields)};

  return ropen(url, hreq, ec);
}

template<typename RequestBody>
struct session::async_ropen_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session * this_;

  struct request_options opts;
  urls::url_view url;
  core::string_view default_mime_type;

  system::error_code ec_;
  using body_type = RequestBody;

  bool is_secure = (url.scheme_id() == urls::scheme::https)
                || (url.scheme_id() == urls::scheme::wss);

  response_base::history_type history;

  optional<beast::http::request<body_type, http::fields>> hreq;
  beast::http::request<body_type, http::fields> &req;

  using flat_buffer = beast::basic_flat_buffer<container::pmr::polymorphic_allocator<char>>;

  urls::url url_cache;

  async_ropen_op(session * this_,
                 http::request<RequestBody> * req,
                 request_options opt, cookie_jar * jar) : this_(this_), opts(opts), req(*req)
  {
  }


  template<typename RequestBody_>
  beast::http::request<body_type, http::fields> prepare_request(
      beast::http::verb method,
      urls::pct_string_view path,
      urls::pct_string_view host,
      RequestBody_ && body,
      http::fields fields,
      error_code &ec)
  {
    using body_traits = request_body_traits<std::decay_t<RequestBody_>>;
    if (!is_secure && this_->options_.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::insecure, &loc);
      return {};
    }

    {
      const auto nm = body_traits::default_content_type(body);
      auto itr = fields.find(http::field::content_type);
      if (itr == fields.end() && !nm.empty())
        fields.set(http::field::content_type, nm);
    }

    {
      detail::monotonic_token mv;
      auto cc = this_->jar_.get(host, is_secure, path,  mv);
      if (!cc.empty())
        fields.set(http::field::cookie, cc);
    }

    http::request<body_type> hreq(method, path, 11,
                                  body_traits::make_body(std::forward<RequestBody_>(body), ec),
                                  std::move(fields));

    hreq.set(beast::http::field::host, host);
    hreq.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    hreq.prepare_payload();
    return hreq;
  }


  template<typename RequestBody_>
  async_ropen_op(session * this_,
                 beast::http::verb method,
                 urls::url_view path,
                 RequestBody_ && body,
                 http::fields req)
      : this_(this_), opts(this_->options_), url(path),
        default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
        hreq(prepare_request(method, url.encoded_resource(), url.encoded_host_and_port(), std::forward<RequestBody_>(body), std::move(req), ec_)),
        req(*hreq)
  {
  }

  using completion_signature_type = void(system::error_code, stream);
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream>);

  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream> s) -> stream
  {
    reenter(this)
    {


      if (!is_secure && this_->options_.enforce_tls)
      {
        static constexpr auto loc((BOOST_CURRENT_LOCATION));
        BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
        return stream{get_executor(), nullptr};
      }

      {
        detail::monotonic_token mv;
        auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), mv);
        if (!cc.empty())
          req.set(http::field::cookie, cc);
      }

      req.prepare_payload();
      yield this_->async_get_pool(url, std::move(self));
      if (ec)
        return stream{get_executor(), nullptr};
      yield variant2::get<1>(s)->async_ropen(req, opts, &this_->jar_, std::move(self));

      if (!ec || opts.max_redirects == variant2::get<2>(s).history().size())
        return std::move(variant2::get<2>(s));

      while (ec == error::forbidden_redirect)
      {
        ec.clear();
        if (variant2::get<2>(s).history().empty())
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
          break;
        }
        opts.max_redirects -= variant2::get<2>(s).history().size();
        if (opts.max_redirects == 0)
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
          break ;
        }

        {
          const auto & last = variant2::get<2>(s).history().back();
          auto loc_itr = last.base().find(http::field::location);
          auto rc = last.base().result();
          if (((rc != http::status::moved_permanently)
               && (rc != http::status::found)
               && (rc != http::status::temporary_redirect)
               && (rc != http::status::permanent_redirect))
              || (loc_itr == last.base().end()))
          {
            BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
            break;
          }
          const auto nurl = interpret_location(req.target(), loc_itr->value());
          if (nurl.has_error())
          {
            ec = nurl.error();
            break;
          }

          if (!should_redirect(this_->options_.redirect, url, *nurl))
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

        }

        req.base().target(url.encoded_resource());
        {
          detail::monotonic_token mv;
          auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), mv);
          if (!cc.empty())
            req.base().set(http::field::cookie, cc);
        }
        history.insert(history.end(),
                       std::make_move_iterator(std::move(variant2::get<2>(s)).history().begin()),
                       std::make_move_iterator(std::move(variant2::get<2>(s)).history().end()));
        yield this_->async_get_pool(url, std::move(self));
        if (ec)
          return stream{get_executor(), nullptr};
        yield variant2::get<1>(s)->async_ropen(req, opts, &this_->jar_, std::move(self));
      }
      variant2::get<2>(s).prepend_history(std::move(history));
      return std::move(variant2::get<2>(s));

    }
    return stream{this_->get_executor(), nullptr};
  }
};

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, basic_stream<Executor>))
session::async_ropen(beast::http::verb method,
                                     urls::url_view path,
                                     RequestBody && body,
                                     http::fields req,
                                     CompletionToken && completion_token)
{
  using op_t = async_ropen_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(std::forward<CompletionToken>(completion_token),
                              this, method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, basic_stream<Executor>))
session::async_ropen(urls::url_view url,
                                     http::request<RequestBody> & req,
                                     CompletionToken && completion_token)
{
  using op_t = async_ropen_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(std::forward<CompletionToken>(completion_token), this, url, &req);

}

}
}

#include <boost/asio/unyield.hpp>

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/session.ipp>
#endif

#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
