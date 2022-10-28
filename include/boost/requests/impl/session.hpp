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
template<typename Allocator>
auto basic_session<Executor>::make_request_(beast::http::basic_fields<Allocator> fields)
    -> requests::basic_request<Allocator>
{
  return requests::basic_request<Allocator>{
    fields.get_allocator(),
    std::move(fields),
    options_,
    &jar_
  };
}

template<typename Executor>
auto basic_session<Executor>::make_request_(beast::http::fields fields) -> requests::request
{
  return requests::request{
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
template<typename RequestBody, typename Allocator>
auto basic_session<Executor>::request(
            beast::http::verb method,
            urls::url_view url,
            RequestBody && body,
            http::basic_fields<Allocator> fields,
            system::error_code & ec) -> basic_response<Allocator>
{
  auto req  = make_request_(std::move(fields));
  // the rest is pretty much the same as in connection on purpose
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  using fields_type = beast::http::basic_fields<Allocator>;
  using response_type = basic_response<Allocator> ;
  using res_body = beast::http::basic_dynamic_body<beast::basic_flat_buffer<Allocator>>;

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
    auto cc = req.jar->get(host,  &memres, is_secure, url.encoded_path());
    if (!cc.empty())
      req.fields.set(http::field::cookie, cc);
  }

  beast::http::request<body_type, fields_type> hreq{method, path, 11,
                                                    body_traits::make_body(std::forward<RequestBody>(body), ec),
                                                    std::move(req.fields)};
  hreq.prepare_payload();
  beast::http::response<res_body, fields_type> rres{http::response_header<fields_type>{alloc},
                                                    beast::basic_flat_buffer<Allocator>{alloc}};

  single_request(hreq, rres, ec);
  using response_type = basic_response<Allocator>;

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
      auto cc = req.jar->get(host, &memres, is_secure, url.encoded_path());
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
    }

    // figure this out.
    single_request(hreq, rres, ec);
    rc = rres.base().result();
  }

  res.header = std::move(rres.base());
  res.buffer = std::move(rres.body());

  return res;
}

template<typename Executor>
template<typename Allocator>
auto basic_session<Executor>::download(
    urls::url_view url,
    http::basic_fields<Allocator> fields,
    const filesystem::path & download_path,
    system::error_code & ec) -> basic_response<Allocator>
{
  auto req  = make_request_(std::move(fields));

  using fields_type = beast::http::basic_fields<Allocator>;
  using response_type = basic_response<Allocator> ;

  const auto single_request =
      [&](beast::http::request<http::empty_body, fields_type> & req,
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
  const auto alloc = req.get_allocator();

  response_type res{alloc};

  if (!is_secure && req.opts.enforce_tls)
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    ec.assign(error::insecure, &loc);
    return res;
  }

  http::request<http::empty_body, fields_type> hreq{http::verb::head, path, 11,
                                                    http::empty_body::value_type{}, std::move(req.fields)};

  // set mime-type
  {
    auto hitr = hreq.base().find(http::field::accept);
    if (hitr == hreq.base().end())
    {
      const auto ext = download_path.extension().string();
      const auto & mp = default_mime_type_map();
      auto itr = mp.find(ext);
      if (itr != mp.end())
        hreq.base().set(http::field::accept, itr->second);
    }
  }
  http::response<http::empty_body, fields_type> hres{http::response_header<fields_type>{alloc}};
  hreq.prepare_payload();

  if (req.jar)
  {
    unsigned char buf[4096];
    boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
    auto cc = req.jar->get(host,  &memres, is_secure, path);
    if (!cc.empty())
      hreq.base().set(http::field::cookie, cc);
    else
      hreq.base().erase(http::field::cookie);
  }

  single_request(hreq, hres, ec);
  using response_type = basic_response<Allocator>;

  urls::url url_cache;
  auto rc = hres.base().result();
  while (!ec &&
         (req.opts.redirect >= redirect_mode::endpoint)
         && ((rc == http::status::moved_permanently)
             || (rc == http::status::found)
             || (rc == http::status::temporary_redirect)
             || (rc == http::status::permanent_redirect)))
  {

    auto loc_itr = hres.base().find(http::field::location);
    if (loc_itr == hres.base().end())
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
      res.header = std::move(hres.base());
      return res;
    }
    const auto nurl = interpret_location(hreq.target(), loc_itr->value());
    if (nurl.has_error())
    {
      ec = nurl.error();
      res.header = std::move(hres.base());
      return res;
    }

    if (!should_redirect(options_.redirect, url, *nurl))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      return res;
    }

    if (nurl->has_authority())
      url = *nurl;
    else
    {
      url_cache = url;
      url_cache.set_encoded_path(nurl->encoded_path());
      url = url_cache;
    }


    if (--req.opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      res.header = std::move(hres.base());
      return res;
    }
    res.history.emplace_back(std::move(hres.base()), (typename response_type::body_type::value_type){alloc});
    hreq.base().target(url.encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = req.jar->get(host, &memres, is_secure, url.encoded_path());
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
      else
        hreq.base().erase(http::field::cookie);
    }

    single_request(hreq, hres, ec);

    rc = hres.base().result();

  }
  hreq.method(beast::http::verb::get);
  http::response<http::file_body, fields_type> fres{http::response_header<fields_type>{alloc}};

  auto str = download_path.string();
  if (!ec)
    fres.body().open(str.c_str(), beast::file_mode::write, ec);
  if (!ec)
    single_request(hreq, fres, ec);


  res.header = std::move(fres.base());

  return res;
}

namespace detail
{

template<typename Executor, typename Request, typename Response>
struct async_single_request_op
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

template<typename Executor, typename Request, typename Response,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
async_single_request(basic_session<Executor>* this_, Request & req, Response & res, urls::url_view url,
                     CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_single_request_op<Executor, Request, Response>{this_, req, res, url},
      completion_token,
      this_->get_executor()
  );
}

}

template<typename Executor>
template<typename RequestBody, typename Allocator>
struct basic_session<Executor>::async_request_op : asio::coroutine
{

  basic_session<Executor> * this_;
  beast::http::verb method;

  struct options opts;
  urls::url_view url;
  core::string_view default_mime_type;

  system::error_code ec_;
  using body_type = RequestBody;

  typename body_type::value_type body;
  http::basic_fields<Allocator> req;

  const bool is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);


  struct state_t
  {
    Allocator alloc;


    //using body_traits = request_body_traits<std::decay_t<RequestBody>>;
    using fields_type = beast::http::basic_fields<Allocator>;
    using response_type = basic_response<Allocator> ;
    response_type res;

    beast::http::request<body_type, fields_type> hreq;
    using res_body = beast::http::basic_dynamic_body<beast::basic_flat_buffer<Allocator>>;
    beast::http::response<res_body, fields_type> rres{http::response_header<fields_type>{alloc},
                                                      beast::basic_flat_buffer<Allocator>{alloc}};

    urls::url url_cache;

    template<typename RequestBody_>
    state_t(beast::http::verb v,
            urls::pct_string_view path,
            RequestBody_ && body,
            requests::basic_request<Allocator> req)
        :  alloc(req.get_allocator()),
          res{req.get_allocator()},
          hreq{v, path, 11,
               std::forward<RequestBody_>(body),
               std::move(req.fields)}
    {
    }
  };

  std::shared_ptr<state_t> state_;

  template<typename RequestBody_>
  async_request_op(basic_session<Executor> * this_,
                   beast::http::verb v,
                   urls::url_view url,
                   RequestBody_ && body,
                   basic_request<Allocator> req)
      : this_(this_), method(v), opts(this_->options_), url(url),
        default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
        body(request_body_traits<std::decay_t<RequestBody_>>::make_body(std::forward<RequestBody_>(body), ec_)),
        req(std::move(req))
  {
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
      auto itr = state_->hreq.base().find(http::field::content_type);
      if (itr == state_->hreq.base().end() && !default_mime_type.empty()) {
        if (!default_mime_type.empty())
          state_->hreq.base().set(http::field::content_type, default_mime_type);
      }
    }

    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = this_->jar_.get(url.encoded_host(),  &memres, is_secure, url.encoded_target());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }
    state_->hreq.prepare_payload();
  }


  void handle_redirect(system::error_code & ec)
  {
    auto loc_itr = state_->rres.base().find(http::field::location);

    if (loc_itr == state_->rres.base().end())
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
      return;
    }
    const auto nurl = interpret_location(state_->hreq.target(), loc_itr->value());
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
      state_->url_cache = url;
      state_->url_cache.set_encoded_path(nurl->encoded_path());
      state_->url_cache.set_encoded_query(nurl->encoded_query());
      url = state_->url_cache;
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
      auto cc = this_->jar_.get(url.encoded_host(), &memres, is_secure, url.encoded_path());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }

    state_->res.history.emplace_back(std::move(state_->rres.base()));
    state_->hreq.base().target(url.encoded_path());
  }


  using conn_type = variant2::variant<
      std::shared_ptr<typename basic_http_connection_pool<Executor>::connection_type>,
      std::shared_ptr<typename basic_https_connection_pool<Executor>::connection_type>>;

  template<typename Self>
  void operator()(Self && self, system::error_code ec= {})
  {
    auto rc = state_ ?  state_->rres.base().result() : http::status();

    reenter(this)
    {
      ec = ec_;
      if (!ec)
      {
        state_ = std::allocate_shared<state_t>(self.get_allocator(),
                                               method, url.encoded_resource(), std::move(body),
                                               this_->make_request_(std::move(req)));
        prepare_initial_head_request(ec);
      }
      if (ec)
      {
        yield asio::post(std::move(self));
        goto complete ;
      }
      yield detail::async_single_request(this_, state_->hreq, state_->rres, url, std::move(self));
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

        yield detail::async_single_request(this_, state_->hreq, state_->rres, url, std::move(self));
      }

      state_->res.buffer = std::move(state_->rres.body());
    complete:
      state_->res.header = std::move(state_->rres.base());
      auto res = std::move(state_->res);
      state_ = nullptr;
      self.complete(ec, std::move(res));
    }
  }
};


#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::empty_body,  std::allocator<char>>;
extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::string_body, std::allocator<char>>;

extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::empty_body,  container::pmr::polymorphic_allocator<char>>;
extern template struct basic_session<asio::any_io_executor>::async_request_op<beast::http::string_body, container::pmr::polymorphic_allocator<char>>;

#endif

template<typename Executor>
template<typename RequestBody, typename Allocator,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               basic_response<Allocator>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        basic_response<Allocator>))
basic_session<Executor>::async_request(beast::http::verb method,
                                       urls::url_view path,
                                       RequestBody && body,
                                       basic_request<Allocator> req,
                                       CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, basic_response<Allocator>)>(
      async_request_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type, Allocator>{
            this, method, path, std::forward<RequestBody>(body), std::move(req)},
      completion_token,
      mutex_
  );
}


template<typename Executor>
template<typename Allocator>
struct basic_session<Executor>::async_download_op : asio::coroutine
{
  using fields_type = beast::http::basic_fields<Allocator>;
  using response_type = basic_response<Allocator> ;

  basic_session<Executor>* this_;
  struct options opts;
  urls::url_view url;
  core::string_view default_mime_type;
  filesystem::path download_path;

  basic_request<Allocator> req;

  const bool is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);

  struct state_t
  {
    Allocator alloc;
    response_type res{alloc};
    beast::http::request<http::empty_body, fields_type> hreq;
    http::response<http::empty_body, fields_type> hres{http::response_header<fields_type>{alloc}};
    http::response<http::file_body, fields_type> fres{http::response_header<fields_type>{alloc}};

    urls::url url_cache;

    state_t(urls::pct_string_view path,
            http::basic_fields<Allocator> fields)
        :  alloc(fields.get_allocator()),
          hreq{http::verb::head, path, 11,
               http::empty_body::value_type{},
               std::move(fields)}
    {
    }

  };


  std::shared_ptr<state_t> state_;

  template<typename ... Args>
  async_download_op(basic_session<Executor> * this_,
                    urls::url_view url,
                    const filesystem::path & download_path,
                    http::basic_fields<Allocator> req)
      : this_(this_), opts(this_->options_), url(url),
        download_path(download_path)
  {
  }

  void prepare_initial_request(system::error_code &ec)
  {
    if (ec)
      return ;
    if (!is_secure && opts.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::insecure, &loc);
      return ;
    }

    // set mime-type
    {
      auto hitr = state_->hreq.base().find(http::field::accept);
      if (hitr == state_->hreq.base().end())
      {
        const auto ext = download_path.extension().string();
        const auto &mp = default_mime_type_map();
        auto itr = mp.find(ext);
        if (itr != mp.end())
          state_->hreq.base().set(http::field::accept, itr->second);
      }
    }

    state_->hreq.prepare_payload();

    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = this_->jar_.get(url.encoded_path(), &memres, is_secure, url.encoded_target());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }
  }

  void handle_redirect(system::error_code & ec)
  {
    auto loc_itr = state_->hres.base().find(http::field::location);
    if (loc_itr == state_->hres.base().end())
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::invalid_redirect, &loc);
      state_->res.header = std::move(state_->hres);
      return ;
    }
    const auto nurl = interpret_location(state_->hreq.target(), loc_itr->value());
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
      state_->url_cache = url;
      state_->url_cache.set_encoded_path(nurl->encoded_path());
      state_->url_cache.set_encoded_query(nurl->encoded_query());
      url = state_->url_cache;
    }

    if (--opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      state_->res.header = std::move(state_->hres);
      return ;
    }
    state_->res.history.emplace_back(std::move(state_->hres.base()));

    state_->hreq.base().target(url.encoded_path());

    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = this_->jar_.get(url.encoded_resource(), &memres, is_secure, url.encoded_path());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }
  }

  template<typename Self>
  void operator()(Self && self, boost::system::error_code ec = {})
  {
    const auto rc = state_ ? state_->hres.result() : http::status();

    reenter(this)
    {
      state_ = std::allocate_shared<state_t>(self.get_allocator(), url.encoded_resource(), std::move(req));
      prepare_initial_request(ec);
      if (ec)
      {
        yield asio::post(asio::append(std::move(self), ec));
        return self.complete(ec, std::move(state_->res));
      }

      yield detail::async_single_request(this_, state_->hreq, state_->hres, url, std::move(self));

      if (ec)
        return self.complete(ec, std::move(state_->res));

      while ((opts.redirect >= redirect_mode::endpoint) &&
             ((rc == http::status::moved_permanently) || (rc == http::status::found) ||
              (rc == http::status::temporary_redirect) || (rc == http::status::permanent_redirect)))
      {
        handle_redirect(ec);
        if (ec)
          goto complete;
        yield detail::async_single_request(this_, state_->hreq, state_->hres, url, std::move(self));

      }

      if (!ec)
      {
        {
          auto str = download_path.string();
          state_->hreq.method(beast::http::verb::get);
          state_->fres.body().open(str.c_str(), beast::file_mode::write, ec);
        }
        if (ec)
          goto complete;

        yield detail::async_single_request(this_, state_->hreq, state_->fres, url, std::move(self));
        state_->res.header = std::move(state_->fres.base());
        auto res = std::move(state_->res);
        state_ = nullptr;
        return self.complete(ec, std::move(res));

      }
      else complete:
      {
        auto res = std::move(state_->res);
        state_ = {};
        return self.complete(ec, std::move(res));
      }
    }
  }

};

#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_session<asio::any_io_executor>::async_download_op<std::allocator<char>>;
extern template struct basic_session<asio::any_io_executor>::async_download_op<container::pmr::polymorphic_allocator<void>>;

#endif


template<typename Executor>
template<typename Allocator,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               basic_response<Allocator>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        basic_response<Allocator>))
basic_session<Executor>::async_download(urls::url_view path,
                                        basic_request<Allocator> req,
                                        filesystem::path download_path,
                                        CompletionToken && completion_token)

{
  return asio::async_compose<CompletionToken, void(system::error_code, basic_response<Allocator>)>(
      async_download_op<Allocator>{this, path, download_path, std::move(req)},
      completion_token,
      mutex_
  );
}

template<typename Executor>
struct basic_session<Executor>::async_get_pool_op : asio::coroutine
{
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
template<typename Allocator,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
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


}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
