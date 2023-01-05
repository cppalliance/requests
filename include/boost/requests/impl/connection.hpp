//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_HPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/async_coroutine.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/ssl.hpp>
#include <boost/requests/fields/location.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/requests/keep_alive.hpp>

#include <boost/asem/lock_guard.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/yield.hpp>
#include <boost/beast/core/buffer_ref.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/core/exchange.hpp>
#include <boost/optional.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/url/grammar/ci_string.hpp>


namespace boost {
namespace requests {



struct connection::async_connect_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection * this_;
  endpoint_type ep;

  asio::coroutine inner_coro;

  async_connect_op(connection * this_, endpoint_type ep) : this_(this_), ep(ep) {}

  using mutex_type = detail::basic_mutex<executor_type>;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;

  lock_type read_lock, write_lock;

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code);

  BOOST_REQUESTS_DECL
  void resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec);
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code))
connection::async_connect(endpoint_type ep, CompletionToken && completion_token)
{

  return detail::co_run<async_connect_op>(
      std::forward<CompletionToken>(completion_token), this, ep);
}

struct connection::async_close_op : asio::coroutine
{
  connection * this_;
  detail::tracker t{this_->ongoing_requests_};

  using executor_type = asio::any_io_executor;
  executor_type get_executor() const {return this_->get_executor();}

  using lock_type = asem::lock_guard<asem::mt::mutex>;
  lock_type read_lock, write_lock;


  async_close_op(connection * this_) : this_(this_) {}

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code);

  BOOST_REQUESTS_DECL
  void resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec);
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
connection::async_close(CompletionToken && completion_token)
{
  return detail::co_run<async_close_op>(
      std::forward<CompletionToken>(completion_token), this);
}

namespace detail
{

struct drop_size_t
{
  auto operator()(system::error_code ec, std::size_t n) -> asio::deferred_values<system::error_code>
  {
    return asio::deferred.values(std::move(ec));
  }
};
static asio::deferred_function<drop_size_t> drop_size()
{
  return asio::deferred(drop_size_t{});
}

}

template<typename Body>
void connection::write_impl(
    http::request<Body> & req,
    asem::lock_guard<asem::mt::mutex> & read_lock,
    system::error_code & ec)
{
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  write_mtx_.lock(ec);
  if (ec)
    return;

  lock_type lock{write_mtx_, std::adopt_lock};
  boost::optional<lock_type> alock;

  // disconnect first
  if (!is_open() && keep_alive_set_.timeout < std::chrono::system_clock::now())
  {
    read_mtx_.lock(ec);
    if (ec)
      return;
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
        return ;
      alock.emplace(read_mtx_, std::adopt_lock);
    }

    next_layer_.next_layer().connect(endpoint_, ec);
    if (use_ssl_ && !ec)
      next_layer_.handshake(asio::ssl::stream_base::client, ec);
    if (ec)
      return ;
  }

  alock.reset();
  if (use_ssl_)
    beast::http::write(next_layer_, req, ec);
  else
    beast::http::write(next_layer_.next_layer(), req, ec);


  if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
    goto retry ;
  else  if (ec)
    return ;

  // release after acquire!
  read_mtx_.lock(ec);

  if (ec)
    return ;

  read_lock = {read_mtx_, std::adopt_lock};
  lock = {};
}


template<typename Body>
auto connection::ropen(http::request<Body> & req,
                       request_options opt,
                       cookie_jar * jar,
                       system::error_code & ec) -> stream
{
  const auto is_secure = use_ssl_;
  using body_type = Body;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  detail::tracker t{this->ongoing_requests_};
  lock_type read_lock;
  if (jar)
  {
    detail::monotonic_token mv;
    auto cc = jar->get(host(), is_secure, req.target(), mv);
    if (!cc.empty())
      req.set(http::field::cookie, cc);
  }

  req.set(http::field::host, host_);
  if (req.count(http::field::user_agent) > 0)
    req.set(http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);
  req.prepare_payload();

  response_base::history_type history;

  while (!ec)
  {
    write_impl(req, read_lock, ec); // <- grabs the read-lock, too.
    if (ec)
      return stream{get_executor(), this};

    stream str{get_executor(), this};
    str.parser_ = detail::make_pmr<http::response_parser<http::buffer_body>>(req.get_allocator().resource(),
                                                                            http::response_header{http::fields(req.get_allocator())});

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
    response_base::buffer_type buf{req.get_allocator()};
    if (req.method() != http::verb::head)
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

    const auto url = interpret_location(req.target(), loc_itr->value());
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

    req.base().target(url->encoded_resource());
    if (jar)
    {
      detail::monotonic_token mv;
      auto cc = jar->get(host(), is_secure, url->encoded_path(), mv);

      if (!cc.empty())
        req.base().set(http::field::cookie, cc);
    }

    req.prepare_payload();
    read_lock = {};
  }

  stream str{get_executor(), this};
  str.history_ = std::move(history);
  return str;
}

namespace detail
{


inline bool check_endpoint(
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

inline bool check_endpoint(
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

inline bool check_endpoint(
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

template<typename RequestBody>
auto connection::ropen(beast::http::verb method,
           urls::url_view path,
           RequestBody && body,
           request_settings req) -> stream
{
  system::error_code ec;
  auto res = ropen(method, path, std::forward<RequestBody>(body), std::move(req), ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}
template<typename RequestBody>
auto connection::ropen(beast::http::verb method,
           urls::url_view path,
           http::request<RequestBody> & req,
           request_options opt,
           cookie_jar * jar) -> stream
{
  system::error_code ec;
  auto res = ropen(method, path, req, std::move(opt), jar, ec);
  if (ec)
    throw_exception(system::system_error(ec));
  return res;
}


template<typename RequestBody>
auto connection::ropen(
    beast::http::verb method,
    urls::url_view path,
    RequestBody && body,
    request_settings req,
    system::error_code & ec) -> stream
{
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  const auto is_secure = use_ssl_;

  if (!detail::check_endpoint(path, endpoint_, host_, use_ssl_, ec))
    return stream{get_executor(), this};

  if (((endpoint_.protocol() == asio::ip::tcp::v4())
    || (endpoint_.protocol() == asio::ip::tcp::v6()))
      && !is_secure && req.opts.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), this};
  }

  {
    const auto nm = body_traits::default_content_type(body);
    auto itr = req.fields.find(http::field::content_type);
    if (itr == req.fields.end() && !nm.empty())
    {
      const auto nm = body_traits::default_content_type(body);
      if (!nm.empty())
        req.fields.set(http::field::content_type, nm);
    }
  }
  auto bd = body_traits::make_body(std::forward<RequestBody>(body), ec);
  if (ec)
    return stream{get_executor(), this};


  beast::http::request<body_type, http::fields> hreq{method, path.encoded_target(), 11,
                                                     std::move(bd),
                                                     std::move(req.fields)};

  return ropen(hreq, std::move(req.opts), req.jar, ec);
}


template<typename RequestBody>
struct connection::async_ropen_op
    : boost::asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  using lock_type = asem::lock_guard<asem::mt::mutex>;

  connection * this_;
  optional<stream> str;

  detail::tracker t{this_->ongoing_requests_};
  using body_type = RequestBody;

  system::error_code ec_;

  request_options opts;
  cookie_jar * jar{nullptr};
  optional<beast::http::request<body_type, http::fields>> hreq;
  beast::http::request<body_type, http::fields> &req;
  response_base::buffer_type buf{req.get_allocator()};

  lock_type lock;
  boost::optional<lock_type> alock;
  asio::coroutine inner_coro;

  response_base::history_type history;


  template<typename RequestBody_>
  beast::http::request<body_type, http::fields> prepare_request(
                       beast::http::verb method,
                       urls::pct_string_view path,
                       core::string_view host,
                       RequestBody_ && body,
                       request_settings req,
                       error_code &ec)
  {
    const auto is_secure = this_->use_ssl_;
    using body_traits = request_body_traits<std::decay_t<RequestBody_>>;
    if (((this_->endpoint().protocol() == asio::ip::tcp::v4())
      || (this_->endpoint().protocol() == asio::ip::tcp::v6()))
      && !is_secure && req.opts.enforce_tls)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
      return {};
    }

    {
      const auto nm = body_traits::default_content_type(body);
      auto itr = req.fields.find(http::field::content_type);
      if (itr == req.fields.end() && !nm.empty())
        req.fields.set(http::field::content_type, nm);
    }

    if (req.jar)
    {
      detail::monotonic_token mv;
      auto cc = req.jar->get(host, is_secure, path,  mv);
      if (!cc.empty())
        req.fields.set(http::field::cookie, cc);
    }

    http::request<body_type> hreq(method, path, 11,
                                  body_traits::make_body(std::forward<RequestBody_>(body), ec),
                                  std::move(req.fields));

    hreq.set(beast::http::field::host, host);
    hreq.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    hreq.prepare_payload();
    return hreq;
  }

  async_ropen_op(connection * this_,
                 http::request<RequestBody> * req,
                 request_options opt, cookie_jar * jar) : this_(this_), opts(opt), jar(jar), req(*req)
  {
  }

  template<typename RequestBody_>
  async_ropen_op(connection * this_,
                 beast::http::verb method,
                 urls::url_view path,
                 RequestBody_ && body,
                 request_settings req)
      : this_(this_), opts(req.opts), jar(req.jar),
        hreq(prepare_request(method, path.encoded_target(), this_->host(), std::forward<RequestBody_>(body), std::move(req), ec_)),
        req(*hreq)
  {
    detail::check_endpoint(path, this_->endpoint(), this_->host(), this_->use_ssl_, ec_);
  }

  using completion_signature_type = void(system::error_code, stream);
  using step_signature_type       = void(system::error_code, std::size_t);

  auto resume(requests::detail::co_token_t<step_signature_type> self, system::error_code & ec,
              std::size_t res_ = 0u) -> stream
  {
    reenter(this)
    {
      if (ec_)
      {
        yield asio::post(this_->get_executor(), std::move(self));
        ec = ec_;
      }

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
          yield beast::http::async_write(this_->next_layer_, req, std::move(self));
        }
        else
        {
          yield beast::http::async_write(this_->next_layer_.next_layer(), req, std::move(self));
        }

        if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
          goto retry ;
        else if (ec)
          break;

        // release after acquire!
        await_lock(this_->read_mtx_, lock);
        // END OF write impl

        str.emplace(this_->get_executor(), this_); // , req.get_allocator().resource()
        str->parser_ = detail::make_pmr<http::response_parser<http::buffer_body>>(req.get_allocator().resource(),
                                                                                  http::response_header{http::fields(req.get_allocator())});

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

        if (req.base().method() != http::verb::head)
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

        const auto url = interpret_location(req.target(), loc_itr->value());
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

        req.base().target(url->encoded_resource());
        if (jar)
        {
          detail::monotonic_token mv;
          auto cc = jar->get(this_->host(), this_->use_ssl_, url->encoded_path(), mv);
          if (!cc.empty())
            req.base().set(http::field::cookie, cc);
        }

        req.prepare_payload();
      }

      stream str{this_->get_executor(), this_};
      str.history_ = std::move(history);
      //t = {};
      return str;

    }
    return stream{this_->get_executor(), this_};

  }
};

struct connection::async_ropen_file_op : async_ropen_op<http::file_body>
{
  using async_ropen_op<http::file_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);

  BOOST_REQUESTS_DECL
  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res = 0u) -> stream;
};


struct connection::async_ropen_string_op : async_ropen_op<http::string_body>
{
  using async_ropen_op<http::string_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);

  BOOST_REQUESTS_DECL
  auto resume(requests::detail::co_token_t<step_signature_type> self,
                system::error_code & ec, std::size_t res = 0u) -> stream;
};

struct connection::async_ropen_empty_op : async_ropen_op<http::empty_body>
{
  using async_ropen_op<http::empty_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);

  BOOST_REQUESTS_DECL
  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res = 0u) -> stream;
};


template<typename RequestBody, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code,
                                                         stream))
connection::async_ropen(
    beast::http::verb method,
    urls::url_view path,
    RequestBody && body,
    request_settings req,
    CompletionToken && completion_token)
{
  using rp = decltype(pick_ropen_op(static_cast<typename request_body_traits<std::decay_t<RequestBody>>::body_type*>(nullptr)));
  return detail::co_run<rp>(
      std::forward<CompletionToken>(completion_token),
      this, method, path,
      std::forward<RequestBody>(body),
      std::move(req));
}

template<typename RequestBody, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        stream))
connection::async_ropen(http::request<RequestBody> & req,
                        request_options opt,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  using rp = async_ropen_op<RequestBody>;
  return detail::co_run<rp>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}

template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection::async_ropen(http::request<http::empty_body> & req,
                        request_options opt,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_empty_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}


template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection::async_ropen(http::request<http::file_body> & req,
                        request_options opt,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_file_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}


template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection::async_ropen(http::request<http::string_body> & req,
                        request_options opt,
                        cookie_jar * jar,
                        CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_string_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}



}
}

#include <boost/asio/unyield.hpp>
#include <boost/requests/detail/undefine.hpp>

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/connection.ipp>
#endif

#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP

