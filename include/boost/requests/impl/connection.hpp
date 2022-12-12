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
namespace detail {

namespace {

template <typename Stream>
void set_verify(Stream *stream, const std::string &host, system::error_code &ec)
{
  stream->set_verify_callback(asio::ssl::host_name_verification(host));
}

void set_verify(std::nullptr_t, const std::string &, system::error_code &) {}

template <typename Stream>
void handshake(Stream *stream, system::error_code &ec)
{
  stream->handshake(asio::ssl::stream_base::client, ec);
}
void handshake(std::nullptr_t, system::error_code &) {}


template <typename Stream>
void shutdown(Stream *stream, system::error_code &ec)
{
  stream->shutdown(ec);
}
void shutdown(std::nullptr_t, system::error_code &) {}


template<typename Stream, typename Ep >
void connect_impl(Stream & stream,
                   Ep ep,
                   system::error_code & ec)
{
  // if it's ssl we assume the host has been set up properly
  beast::get_lowest_layer(stream).connect(ep, ec);
  if (ec)
    return ;

  detail::handshake(detail::get_ssl_layer(stream), ec);
}

template<typename Stream>
void close_impl(Stream & stream,
                system::error_code & ec)
{
  detail::shutdown(detail::get_ssl_layer(stream), ec);
  beast::get_lowest_layer(stream).close(ec);
}


template<typename Stream, typename Ep, typename Token, typename Ssl>
void async_connect_impl(Stream & stream, Ep && ep, Token && token, Ssl * ssl)
{
  using asio::deferred;
  return beast::get_lowest_layer(stream).
      async_connect(std::forward<Ep>(ep),
          deferred(
              [ssl](system::error_code ec)
              {
                return deferred.when(!ec)
                    .then(ssl->async_handshake(asio::ssl::stream_base::client, asio::deferred_t()))
                    .otherwise(deferred.values(ec));
              }))(std::forward<Token>(token));
}

template<typename Stream, typename Ep, typename Token>
void async_connect_impl(Stream & stream, Ep && ep, Token && token, std::nullptr_t)
{
  return beast::get_lowest_layer(stream).async_connect(std::forward<Ep>(ep), std::forward<Token>(token));
}


template<typename Stream, typename Ep, typename Token>
void async_connect_impl(Stream & stream, Ep && ep, Token && token)
{
  return async_connect_impl(stream, std::forward<Ep>(ep),
                            std::forward<Token>(token), get_ssl_layer(stream));
}

template<typename Stream, typename Token, typename Ssl>
auto async_close_impl(Stream & stream, Token && token, Ssl * ssl)
{
  return ssl->async_shutdown(
      asio::deferred(
          [&stream](system::error_code ec)
          {
            beast::get_lowest_layer(stream).close(ec);
            return asio::deferred.values(ec);
          }))
      (std::forward<Token>(token));
}

template<typename Stream, typename Token>
auto async_close_impl(Stream & stream, Token && token, std::nullptr_t)
{
  system::error_code ec;
  beast::get_lowest_layer(stream).close(ec);
  return asio::post(asio::append(std::forward<Token>(token), ec));
}


template<typename Stream, typename Token>
auto async_close_impl(Stream & stream, Token && token)
{
  return async_close_impl(stream, std::forward<Token>(token), get_ssl_layer(stream));
}

}
}

template<typename Stream>
void basic_connection<Stream>::set_host(core::string_view sv, system::error_code & ec)
{
  detail::set_verify(detail::get_ssl_layer(next_layer_), host_ = sv, ec);
}

template<typename Stream>
void basic_connection<Stream>::connect(endpoint_type ep, system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;
  detail::connect_impl(next_layer_, endpoint_ = ep, ec);
}


template<typename Stream>
void basic_connection<Stream>::close(system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;

  detail::close_impl(next_layer_, ec);
}

template<typename Stream>
struct basic_connection<Stream>::async_connect_op : asio::coroutine
{
  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return this_->get_executor(); }

  basic_connection<Stream> * this_;
  endpoint_type ep;

  asio::coroutine inner_coro;

  async_connect_op(basic_connection<Stream> * this_, endpoint_type ep) : this_(this_), ep(ep) {}

  using mutex_type = detail::basic_mutex<executor_type>;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;

  lock_type read_lock, write_lock;

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code);

  void resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec)
  {
    reenter(this)
    {
      await_lock(this_->write_mtx_, write_lock);
      await_lock(this_->read_mtx_,  read_lock);
      yield detail::async_connect_impl(this_->next_layer_, this_->endpoint_ = ep, std::move(self));
    }
  }
};

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code))
basic_connection<Stream>::async_connect(endpoint_type ep, CompletionToken && completion_token)
{

  return detail::co_run<async_connect_op>(
      std::forward<CompletionToken>(completion_token), this, ep);
}

template<typename Stream>
struct basic_connection<Stream>::async_close_op
{
  basic_connection<Stream> * this_;
  detail::tracker t{this_->ongoing_requests_};
  using mutex_type = detail::basic_mutex<executor_type>;
  using lock_type = asem::lock_guard<mutex_type>;

  template<typename Self>
  void operator()(Self && self)
  {
    using namespace asio::experimental;

    make_parallel_group(
        asem::async_lock(this_->write_mtx_, asio::deferred),
        asem::async_lock(this_->read_mtx_, asio::deferred))
        .async_wait(wait_for_all(), asio::append(std::move(self), detail::get_ssl_layer(this_->next_layer_)));
  }

  template<typename Self, typename Stream_>
  void operator()(Self && self,
                  std::array<std::size_t, 2u>,
                  system::error_code ec1,
                  asem::lock_guard<mutex_type> lock1,
                  system::error_code ec2,
                  asem::lock_guard<mutex_type> lock2,
                  Stream_ * stream)
  {
    if (ec1 || ec2)
      self.complete(ec1 ? ec1 : ec2);
    else
      stream->async_shutdown(
          asio::append(std::move(self), std::move(lock1), std::move(lock2)));
  }
  template<typename Self>
  void operator()(Self && self,
                  std::array<std::size_t, 2u>,
                  system::error_code ec1,
                  asem::lock_guard<mutex_type> lock1,
                  system::error_code ec2,
                  asem::lock_guard<mutex_type> lock2,
                  std::nullptr_t)
  {
    (*this)(std::move(self), ec1 ? ec1 : ec2, std::move(lock1), std::move(lock2));
  }


  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec,
                  asem::lock_guard<mutex_type> lock1,
                  asem::lock_guard<mutex_type> lock2)
  {
    if (!ec)
      this_->next_layer_.close(ec);
    self.complete(ec);
  }

};

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
basic_connection<Stream>::async_close(CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_close_op{this},
      completion_token,
      next_layer_
  );
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


template<typename Stream>
template<typename Body>
void basic_connection<Stream>::write_impl(
    http::request<Body> & req,
    asem::lock_guard<detail::basic_mutex<executor_type>> & read_lock,
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
    detail::close_impl(next_layer_, ec);
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
    detail::connect_impl(next_layer_, endpoint_, ec);
    if (ec)
      return ;
  }

  alock.reset();
  beast::http::write(next_layer_, req, ec);

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

template<typename Stream>
template<typename Body>
auto basic_connection<Stream>::ropen(http::request<Body> & req,
                                     request_options opt,
                                     cookie_jar * jar,
                                     system::error_code & ec) -> stream
{
  constexpr auto is_secure = detail::has_ssl_v<Stream>;
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
    beast::http::read_header(next_layer_, buffer_, *str.parser_, ec);
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

template<typename Stream>
auto basic_connection<Stream>::ropen(http::request<http::empty_body> & req,  request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::empty_body>(req, std::move(opt), jar, ec);
}
template<typename Stream>
auto basic_connection<Stream>::ropen(http::request<http::file_body> & req,   request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::file_body>(req, std::move(opt), jar, ec);
}
template<typename Stream>
auto basic_connection<Stream>::ropen(http::request<http::string_body> & req, request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::string_body>(req, std::move(opt), jar, ec);
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

}

template<typename Stream>
template<typename RequestBody>
auto basic_connection<Stream>::ropen(
    beast::http::verb method,
    urls::url_view path,
    RequestBody && body,
    request_settings req,
    system::error_code & ec) -> stream
{
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  constexpr auto is_secure = detail::has_ssl_v<Stream>;

  if (!detail::check_endpoint(path, endpoint_, host_, detail::has_ssl_v<Stream>, ec))
    return stream{get_executor(), this};


  if (std::is_same<protocol_type, asio::ip::tcp>::value && !is_secure && req.opts.enforce_tls)
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


template<typename Stream>
template<typename RequestBody>
struct basic_connection<Stream>::async_ropen_op
    : boost::asio::coroutine
{
  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return this_->get_executor(); }

  using lock_type = asem::lock_guard<detail::basic_mutex<typename Stream::executor_type>>;

  basic_connection<Stream> * this_;
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
    constexpr auto is_secure = detail::has_ssl_v<Stream>;
    using body_traits = request_body_traits<std::decay_t<RequestBody_>>;
    if (std::is_same<protocol_type, asio::ip::tcp>::value && !is_secure && req.opts.enforce_tls)
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

  async_ropen_op(basic_connection<Stream> * this_,
                 http::request<RequestBody> * req,
                 request_options opt, cookie_jar * jar) : this_(this_), opts(opt), jar(jar), req(*req)
  {
  }

  template<typename RequestBody_>
  async_ropen_op(basic_connection<Stream> * this_,
                 beast::http::verb method,
                 urls::url_view path,
                 RequestBody_ && body,
                 request_settings req)
      : this_(this_), opts(req.opts), jar(req.jar),
        hreq(prepare_request(method, path.encoded_target(), this_->host(), std::forward<RequestBody_>(body), std::move(req), ec_)),
        req(*hreq)
  {
    detail::check_endpoint(path, this_->endpoint(), this_->host(), detail::has_ssl_v<Stream>, ec_);
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
          yield detail::async_close_impl(this_->next_layer_, std::move(self));
          ec.clear();
        }


        if (!this_->is_open())
        {
        retry:
          if (!alock)
            await_lock(this_->read_mtx_, alock);
          yield detail::async_connect_impl(this_->next_layer_, this_->endpoint_, std::move(self));
          if (ec)
            break;
        }

        alock.reset();
        yield beast::http::async_write(this_->next_layer_, req, std::move(self));

        if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
          goto retry ;
        else if (ec)
          break;

        // release after acquire!
        await_lock(this_->read_mtx_, lock);
        // END OF write impl
        using base = detail::stream_base;
        str.emplace(this_->get_executor(), static_cast<base*>(this_)); // , req.get_allocator().resource()
        str->parser_ = detail::make_pmr<http::response_parser<http::buffer_body>>(req.get_allocator().resource(),
                                                                                  http::response_header{http::fields(req.get_allocator())});
        yield beast::http::async_read_header(this_->next_layer_, this_->buffer_, *str->parser_, std::move(self));
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
          auto cc = jar->get(this_->host(), detail::has_ssl_v<Stream>, url->encoded_path(), mv);
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



template<typename Stream>
struct basic_connection<Stream>::async_ropen_file_op : async_ropen_op<http::file_body>
{
  using async_ropen_op<http::file_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);
  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res = 0u) -> stream
  {
    return async_ropen_op<http::file_body>::resume(std::move(self), ec, res);
  }
};

template<typename Stream>
struct basic_connection<Stream>::async_ropen_string_op : async_ropen_op<http::string_body>
{
  using async_ropen_op<http::string_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);
    auto resume(requests::detail::co_token_t<step_signature_type> self,
                system::error_code & ec, std::size_t res = 0u) -> stream
  {
    return async_ropen_op<http::string_body>::resume(std::move(self), ec, res);
  }
};

template<typename Stream>
struct basic_connection<Stream>::async_ropen_empty_op : async_ropen_op<http::empty_body>
{
  using async_ropen_op<http::empty_body>::async_ropen_op;
  using step_signature_type       = void(system::error_code, std::size_t);
  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res = 0u) -> stream
  {
    return async_ropen_op<http::empty_body>::resume(std::move(self), ec, res);
  }
};

template<typename Stream>
template<typename RequestBody, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code,
                                                         typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(
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

template<typename Stream>
template<typename RequestBody, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(http::request<RequestBody> & req,
                                      request_options opt,
                                      cookie_jar * jar,
                                      CompletionToken && completion_token)
{
  using rp = async_ropen_op<RequestBody>;
  return detail::co_run<rp>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}

template<typename Stream>
template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(http::request<http::empty_body> & req,
                                      request_options opt,
                                      cookie_jar * jar,
                                      CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_empty_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}


template<typename Stream>
template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(http::request<http::file_body> & req,
                                      request_options opt,
                                      cookie_jar * jar,
                                      CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_file_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}


template<typename Stream>
template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(http::request<http::string_body> & req,
                                      request_options opt,
                                      cookie_jar * jar,
                                      CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_string_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
}




template<typename Stream>
std::size_t basic_connection<Stream>::do_read_some_(beast::http::basic_parser<false> & parser)
{
  return beast::http::read_some(next_layer_, buffer_, parser);
}
template<typename Stream>
std::size_t basic_connection<Stream>::do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec) 
{
  return beast::http::read_some(next_layer_, buffer_, parser, ec);
}

template<typename Stream>
void basic_connection<Stream>::do_async_read_some_(beast::http::basic_parser<false> & parser, detail::co_token_t<void(system::error_code, std::size_t)> tk)
{
  return beast::http::async_read_some(next_layer_, buffer_, parser, std::move(tk));
}

template<typename Stream>
void basic_connection<Stream>::do_close_(system::error_code & ec)
{
  return detail::close_impl(next_layer_, ec);
}
template<typename Stream>
void basic_connection<Stream>::do_async_close_(detail::co_token_t<void(system::error_code)> tk)
{
  return detail::async_close_impl(next_layer_, std::move(tk));
}

}
}

#include <boost/asio/unyield.hpp>
#include <boost/requests/detail/undefine.hpp>

#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP

