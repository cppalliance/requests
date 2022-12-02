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


template<typename Executor>
auto basic_session<Executor>::make_request_(http::fields fields) -> requests::request_settings
{
  return requests::request_settings{
    std::move(fields),
    options_,
    &jar_
  };
}


template<typename Executor>
auto
basic_session<Executor>::get_pool(urls::url_view url, error_code & ec) -> pool_ptr
{
  // can be optimized to ellide the string allocation, blabla (pmr?)
  char buf[1024];
  container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};
  using alloc = container::pmr::polymorphic_allocator<char>;
  using str = std::basic_string<char, std::char_traits<char>, alloc>;
  std::pair<str, unsigned short> host_key{alloc(&res), get_port(url)};
  url.host(urls::string_token::assign_to(host_key.first));

  const auto is_https = (url.scheme_id() == urls::scheme::https)
                     || (url.scheme_id() == urls::scheme::wss);
  auto lock = asem::lock(mutex_, ec);
  if (ec)
    return std::shared_ptr<basic_http_connection_pool<Executor>>();

  if (is_https)
  {
    auto itr = https_pools_.find(host_key);
    if (itr != https_pools_.end())
      return itr->second;
    else
    {
      auto p = std::make_shared<basic_https_connection_pool<Executor>>(get_executor(), sslctx_);
      p->lookup(url.authority(), ec);
      if (!ec)
      {
        https_pools_.emplace(host_key, p);
        return p;
      }
    }
  }
  else
  {
    auto itr = http_pools_.find(host_key);
    if (itr != http_pools_.end())
      return itr->second;
    else
    {
      auto p = std::make_shared<basic_http_connection_pool<Executor>>(get_executor());
      p->lookup(url.authority(), ec);
      if (!ec)
      {
        http_pools_.emplace(host_key, p);
        return p;
      }
    }
  }
  return std::shared_ptr<basic_http_connection_pool<Executor>>(nullptr);
}

template<typename Executor>
template<typename RequestBody>
auto basic_session<Executor>::request(
            beast::http::verb method,
            urls::url_view url,
            RequestBody && body,
            http::fields fields,
            system::error_code & ec) -> response
{
  auto req  = make_request_(std::move(fields));
  // the rest is pretty much the same as in connection on purpose
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  using response_type = response ;
  using flat_buffer = beast::basic_flat_buffer<container::pmr::polymorphic_allocator<char>>;
  using res_body = beast::http::basic_dynamic_body<flat_buffer>;

  const auto single_request =
      [&](auto & req,
          auto & res,
          error_code & ec)
      {
        auto p = get_pool(url, ec);
        if (ec)
          return ;
        visit(
            [&](auto pool)
            {
              assert(pool);
              auto conn = pool->get_connection(ec);
              if (ec)
                return ;
              conn->single_request(req, res, ec);
            }, p);
      };

  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);
  auto path = url.encoded_target();
  auto host = url.encoded_host();
  if (path.empty())
    path = "/";
  const auto alloc = req.get_allocator();
  response_type res{alloc};

  if (!is_secure && req.opts.enforce_tls)
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    ec.assign(error::insecure, &loc);
    return res;
  }

  {
    const auto nm = body_traits::default_content_type(body);
    auto itr = req.fields.find(http::field::content_type);
    if (itr == req.fields.end() && !nm.empty()) {
      const auto nm = body_traits::default_content_type(body);
      if (!nm.empty())
        req.fields.set(http::field::content_type, nm);
    }
  }

  if (req.jar)
  {
    unsigned char buf[4096];
    boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
    container::pmr::polymorphic_allocator<char> alloc2{&memres};
    auto cc = req.jar->get(host, is_secure, url.encoded_path(), alloc2);
    if (!cc.empty())
      req.fields.set(http::field::cookie, cc);
  }

  beast::http::request<body_type, http::fields> hreq{method, path, 11,
                                                    body_traits::make_body(std::forward<RequestBody>(body), ec),
                                                    std::move(req.fields)};
  hreq.prepare_payload();
  beast::http::response<res_body, http::fields> rres{beast::http::response_header<http::fields>{alloc}, flat_buffer{alloc}};

  single_request(hreq, rres, ec);
  using response_type = response;

  urls::url url_cache;
  auto rc = rres.base().result();
  while (!ec &&
        ((rc == http::status::moved_permanently)
      || (rc == http::status::found)
      || (rc == http::status::temporary_redirect)
      || (rc == http::status::permanent_redirect)))
  {
    auto loc_itr = rres.base().find(http::field::location);
    if (loc_itr == rres.base().end())
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
      break ;
    }

    const auto nurl = interpret_location(hreq.target(), loc_itr->value());
    if (nurl.has_error())
    {
      ec = nurl.error();
      break;
    }

    if (!should_redirect(options_.redirect, url, *nurl))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
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

    // we don't need to use should_redirect, bc we're on the same host.
    if (--req.opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      break ;
    }
    res.history.emplace_back(std::move(rres.base()), std::move(rres.body()));

    hreq.base().target(url.encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = req.jar->get(host, is_secure, url.encoded_path(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
    }

    // figure this out.
    single_request(hreq, rres, ec);
    rc = rres.base().result();
  }

  res.headers = std::move(rres.base());
  res.buffer = std::move(rres.body());

  return res;
}


template<typename Executor>
template<typename Request, typename Response>
struct basic_session<Executor>::async_single_request_op
{
  basic_session<Executor>* this_;
  Request & req;
  Response & res;
  urls::url_view url;

  template<typename Self>
  void operator()(Self && self)
  {
    this_->async_get_pool(url, std::move(self));
  }

  template<typename Self>
  void operator()(Self && self, error_code ec,
                  typename basic_session<Executor>::pool_ptr pp)
  {
    if (ec)
      return self.complete(ec);

    visit([&](auto p) {
      p->async_get_connection(std::move(self));
    }, pp);
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, std::shared_ptr<basic_http_connection<Executor>> p)
  {
    if (ec)
      return self.complete(ec);
    p-> async_single_request(req, res, std::move(self));
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, std::shared_ptr<basic_https_connection<Executor>> p)
  {
    if (ec)
      return self.complete(ec);
    p-> async_single_request(req, res, std::move(self));
  }

  template<typename Self>
  void operator()(Self && self, error_code ec)
  {
    return self.complete(ec);
  }
};

template<typename Executor>
template<typename Request, typename Response,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
basic_session<Executor>::async_single_request(
                     Request & req, Response & res, urls::url_view url,
                     CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_single_request_op<Request, Response>{this, req, res, url},
      completion_token,
      get_executor()
  );
}



template<typename Executor>
template<typename RequestBody>
struct basic_session<Executor>::async_request_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_session<Executor> * this_;
  beast::http::verb method;

  struct request_options opts;
  urls::url_view url;
  core::string_view default_mime_type;

  system::error_code ec_;
  using body_type = RequestBody;

  bool is_secure = (url.scheme_id() == urls::scheme::https)
                || (url.scheme_id() == urls::scheme::wss);

  using response_type = response ;
  response_type res;


  http::request<body_type> hreq;
  using flat_buffer = beast::basic_flat_buffer<container::pmr::polymorphic_allocator<char>>;
  using res_body = beast::http::basic_dynamic_body<flat_buffer>;
  http::response<res_body> rres{beast::http::response_header<http::fields>{hreq.get_allocator()},
                                flat_buffer{hreq.get_allocator()}};
  urls::url url_cache;

  template<typename RequestBody_>
  async_request_op(basic_session<Executor> * this_,
                   beast::http::verb v,
                   urls::url_view url,
                   RequestBody_ && body,
                   http::fields req)
      : this_(this_), method(v), opts(this_->options_), url(url),
        default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
        res{req.get_allocator()},
        hreq{v, url.encoded_resource(), 11,
             request_body_traits<std::decay_t<RequestBody_>>::make_body(std::forward<RequestBody_>(body), ec_),
             std::move(req)}

  {
  }


  template<typename RequestBody_>
  async_request_op(basic_session<Executor> * this_,
                   beast::http::verb v,
                   core::string_view url,
                   RequestBody_ && body,
                   http::fields req)
      : this_(this_), method(v), opts(this_->options_),
        default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
        res{req.get_allocator()},
        hreq{v, "", 11,
             request_body_traits<std::decay_t<RequestBody_>>::make_body(std::forward<RequestBody_>(body), ec_),
             std::move(req)}
  {
    auto u = urls::parse_uri(url);
    if (u.has_error())
      ec_ = u.error();
    else
    {
      this->url = u.value();
      is_secure = (this->url.scheme_id() == urls::scheme::https)
               || (this->url.scheme_id() == urls::scheme::wss);
    }
    hreq.target(this->url.encoded_resource());

  }

  void prepare_initial_head_request(error_code & ec)
  {
    if (ec)
      return ;
    if (!is_secure && opts.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec = system::error_code(error::insecure, &loc);
      return ;
    }

    {
      auto itr = hreq.base().find(http::field::content_type);
      if (itr == hreq.base().end() && !default_mime_type.empty()) {
        if (!default_mime_type.empty())
          hreq.base().set(http::field::content_type, default_mime_type);
      }
    }

    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_target(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
      else
        hreq.base().erase(http::field::cookie);
    }
    hreq.prepare_payload();
  }


  void handle_redirect(system::error_code & ec)
  {
    auto loc_itr = rres.base().find(http::field::location);

    if (loc_itr == rres.base().end())
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
      return;
    }
    const auto nurl = interpret_location(hreq.target(), loc_itr->value());
    if (nurl.has_error())
    {
      ec = nurl.error();
      return;
    }

    if (!should_redirect(opts.redirect, url, *nurl))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      return ;
    }

    if (nurl->has_authority())
      url = *nurl;
    else
    {
      url_cache = url;
      url_cache.set_encoded_path(nurl->encoded_path());
      url_cache.set_encoded_query(nurl->encoded_query());
      url = url_cache;
    }


    if (--opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      return;
    }

    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};

      auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
      else
        hreq.base().erase(http::field::cookie);
    }

    res.history.emplace_back(std::move(rres.base()));
    hreq.base().target(url.encoded_path());
  }


  using conn_type = variant2::variant<
      std::shared_ptr<typename basic_http_connection_pool<Executor>::connection_type>,
      std::shared_ptr<typename basic_https_connection_pool<Executor>::connection_type>>;

  using completion_signature_type = void(system::error_code, response);
  using step_signature_type       = void(system::error_code);

  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec) -> response &
  {
    auto rc = rres.base().result();

    reenter(this)
    {
      if (!ec_)
        prepare_initial_head_request(ec_);
      if (ec_)
      {
        yield asio::post(std::move(self));
        ec = ec_;
        goto complete ;
      }
      yield this_->async_single_request(hreq, rres, url, std::move(self));
      while (!ec &&
             (opts.redirect >= redirect_mode::endpoint)
             && ((rc == http::status::moved_permanently)
                 || (rc == http::status::found)
                 || (rc == http::status::temporary_redirect)
                 || (rc == http::status::permanent_redirect)))
      {
        handle_redirect(ec);
        if (ec)
          goto complete;

        yield this_->async_single_request(hreq, rres, url, std::move(self));
      }

      res.buffer = std::move(rres.body());
    complete:
      res.headers = std::move(rres.base());
    }
    return res;
  }
};


#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::empty_body >;
extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::string_body>;

#endif

template<typename Executor>
template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
basic_session<Executor>::async_request(beast::http::verb method,
                                       urls::url_view path,
                                       RequestBody && body,
                                       http::fields req,
                                       CompletionToken && completion_token)
{
  using op_t = async_request_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(
      std::forward<CompletionToken>(completion_token),
      this, method, path, std::forward<RequestBody>(body), std::move(req));
}


template<typename Executor>
struct basic_session<Executor>::async_get_pool_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_session<Executor> *this_;
  struct impl_t
  {
    char buf[1024];
    container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};
    using alloc = container::pmr::polymorphic_allocator<char>;
    using str = std::basic_string<char, std::char_traits<char>, alloc>;
    std::pair<str, unsigned short> host_key;
    const bool is_https;

    impl_t(urls::url_view url)
        : host_key{alloc(&res), get_port(url)}
        , is_https((url.scheme_id() == urls::scheme::https) || (url.scheme_id() == urls::scheme::wss))

    {
      url.host(urls::string_token::assign_to(host_key.first));
    }
  };

  urls::url_view url;
  std::shared_ptr<impl_t> impl;

  template<typename Self>
  void complete(Self && self,
                error_code ec,
                pool_ptr pp,
                asem::lock_guard<detail::basic_mutex<executor_type>> &lock)
  {
    impl = {};
    lock = {};
    self.complete(ec, std::move(pp));
  }

  std::shared_ptr<basic_http_connection_pool <Executor>> p;
  std::shared_ptr<basic_https_connection_pool<Executor>> ps;

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

      if (impl->is_https)
      {
        {
          auto itr = this_->https_pools_.find(impl->host_key);
          if (itr != this_->https_pools_.end())
            return complete(std::move(self), {}, itr->second, lock);
        }
        ps = std::make_shared<basic_https_connection_pool<Executor>>(this_->get_executor(), this_->sslctx_);
        yield ps->async_lookup(url.authority(), asio::append(std::move(self), std::move(lock)));
        if (!ec)
        {
          this_->https_pools_.emplace(impl->host_key, ps);
          return complete(std::move(self), {}, std::move(ps), lock);
        }
      }
      else
      {
        {
          auto itr = this_->http_pools_.find(impl->host_key);
          if (itr != this_->http_pools_.end())
            return complete(std::move(self), {}, itr->second, lock);
        }
        p = std::make_shared<basic_http_connection_pool<Executor>>(this_->get_executor());
        yield p->async_lookup(url.authority(), asio::append(std::move(self), std::move(lock)));
        if (!ec)
        {
          this_->http_pools_.emplace(impl->host_key, p);
          return complete(std::move(self), {}, std::move(p), lock);
        }
      }
      return complete(std::move(self), ec, {}, lock);
    }
  }
};

template<typename Executor>
template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               variant2::variant<std::shared_ptr<basic_http_connection_pool<Executor>>,
                                                                 std::shared_ptr<basic_https_connection_pool<Executor>>>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
basic_session<Executor>::async_get_pool(urls::url_view url,
                                        CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, pool_ptr)>(
      async_get_pool_op{{}, this, url},
      completion_token,
      mutex_
  );
}

template<typename Executor>
template<typename Body>
auto basic_session<Executor>::ropen(
           urls::url_view url,
           http::request<Body>& req,
           system::error_code & ec) -> stream
{
  auto opts = options_;

  response_base::history_type history{req.get_allocator()};

  if (!url.encoded_target().empty() && req.target().empty())
    req.target(url.encoded_target());

  auto do_ropen =
      [&](http::request<Body> & req, request_options opts) -> stream
      {
        auto p = get_pool(url, ec);
        if (ec)
          return stream{get_executor(), nullptr};

        return visit(
            [&](auto pool) -> stream
            {
              assert(pool);
              return pool->ropen( req, opts, &jar_, ec);
            }, p);
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
    unsigned char buf[4096];
    boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
    container::pmr::polymorphic_allocator<char> alloc2{&memres};
    auto cc = jar_.get(host, is_secure, url.encoded_path(), alloc2);
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

    req.base().target(url.encoded_path());
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = jar_.get(host, is_secure, url.encoded_path(), alloc2);
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

template<typename Executor>
template<typename RequestBody>
auto basic_session<Executor>::ropen(
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


  beast::http::request<body_type, http::fields> hreq{method, url.encoded_target(), 11,
                                                     std::move(bd),
                                                     std::move(fields)};

  return ropen(url, hreq, ec);
}

template<typename Executor>
template<typename RequestBody>
struct basic_session<Executor>::async_ropen_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_session<Executor> * this_;

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

  async_ropen_op(basic_session<Executor> * this_,
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
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
      auto cc = this_->jar_.template get<allocator_type>(host, is_secure, path,  &memres);
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
  async_ropen_op(basic_session<Executor> * this_,
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
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate, pool_ptr, stream>);

  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, variant2::variant<variant2::monostate, pool_ptr, stream> s) -> stream
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
        unsigned char buf[4096];
        boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
        container::pmr::polymorphic_allocator<char> alloc2{&memres};
        auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), alloc2);
        if (!cc.empty())
          req.set(http::field::cookie, cc);
      }

      req.prepare_payload();
      yield this_->async_get_pool(url, std::move(self));
      if (ec)
        return basic_stream<Executor>{get_executor(), nullptr};
      yield visit(
          [&](auto pool)
          {
            pool->async_ropen(req, opts, &this_->jar_, std::move(self));
          }, variant2::get<1>(s));

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

        req.base().target(url.encoded_path());
        {
          unsigned char buf[4096];
          boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
          container::pmr::polymorphic_allocator<char> alloc2{&memres};
          auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), alloc2);
          if (!cc.empty())
            req.base().set(http::field::cookie, cc);
        }
        history.insert(history.end(),
                       std::make_move_iterator(std::move(variant2::get<2>(s)).history().begin()),
                       std::make_move_iterator(std::move(variant2::get<2>(s)).history().end()));
        yield this_->async_get_pool(url, std::move(self));
        if (ec)
          return stream{get_executor(), nullptr};
        yield visit(
            [&](auto pool)
            {
              pool->async_ropen(req, opts, &this_->jar_, std::move(self));
            }, variant2::get<1>(s));
      }
      variant2::get<2>(s).prepend_history(std::move(history));
      return std::move(variant2::get<2>(s));

    }
    return stream{this_->get_executor(), nullptr};
  }
};

template<typename Executor>
template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, basic_stream<Executor>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, basic_stream<Executor>))
basic_session<Executor>::async_ropen(beast::http::verb method,
                                     urls::url_view path,
                                     RequestBody && body,
                                     http::fields req,
                                     CompletionToken && completion_token)
{
  using op_t = async_ropen_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(std::forward<CompletionToken>(completion_token),
                              this, method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename Executor>
template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, basic_stream<Executor>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, basic_stream<Executor>))
basic_session<Executor>::async_ropen(urls::url_view url,
                                     http::request<RequestBody> & req,
                                     CompletionToken && completion_token)
{
  using op_t = async_ropen_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(std::forward<CompletionToken>(completion_token), this, url, &req);

}

}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
