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
template<typename RequestBody,
          typename ResponseBody>
void basic_connection<Stream>::single_request(
    http::request<RequestBody>   &req,
    http::response<ResponseBody> &res,
    system::error_code & ec)
{
  detail::tracker t{ongoing_requests_};
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
        return;
      alock.emplace(read_mtx_, std::adopt_lock);
    }
    detail::connect_impl(next_layer_, endpoint_, ec);
    if (ec)
      return;
  }

  alock.reset();

  req.set(beast::http::field::host, host_);
  req.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

  beast::http::write(next_layer_, req, ec);
  if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
    goto retry ;
  else  if (ec)
    return ;

  // release after acquire!
  read_mtx_.lock(ec);

  if (ec)
    return;
  lock = {read_mtx_, std::adopt_lock};

  // DO the read - the head needs to be handled differently, because it has a content-length but no body.
  if (req.base().method() == beast::http::verb::head)
  {
    http::response_parser<ResponseBody> ps{std::move(res.base())};
    beast::http::read_header(next_layer_, buffer_, ps, ec);

    res = std::move(ps.get());
  }
  else
    beast::http::read(next_layer_, buffer_, res, ec);

  if (ec)
    return ;

  bool should_close = interpret_keep_alive_response(keep_alive_set_, res, ec);
  if (should_close)
  {
    boost::system::error_code ec_;
    write_mtx_.lock(ec_);
    if (ec_)
      return;
    alock.emplace(write_mtx_, std::adopt_lock);
    if (!ec_)
      detail::close_impl(next_layer_, ec_);
    return;
  }
}

template<typename Stream>
template<typename RequestBody, typename ResponseBody>
void basic_connection<Stream>::single_header_request(
    http::request<RequestBody> &req,
    http::response_parser<ResponseBody> & res,
    system::error_code & ec)
{
  detail::tracker t{ongoing_requests_};
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
        return;
      alock.emplace(read_mtx_, std::adopt_lock);
    }
    detail::connect_impl(next_layer_, endpoint_, ec);
    if (ec)
      return;
  }

  alock.reset();

  req.set(beast::http::field::host, host_);
  req.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

  beast::http::write(next_layer_, req, ec);
  if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
    goto retry ;
  else  if (ec)
    return ;

  // release after acquire!
  read_mtx_.lock(ec);

  if (ec)
    return;
  lock = {read_mtx_, std::adopt_lock};

  // DO the read - the head needs to be handled differently, because it has a content-length but no body.
  beast::http::read_header(next_layer_, buffer_, res, ec);
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
template<typename RequestBody,
         typename ResponseBody>
struct basic_connection<Stream>::async_single_request_op
    : boost::asio::coroutine
{

  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return this_->get_executor(); }

  using lock_type = asem::lock_guard<detail::basic_mutex<typename Stream::executor_type>>;

  basic_connection<Stream> * this_;
  http::request<RequestBody> &req;
  http::response<ResponseBody> & res;
  boost::system::error_code ec_;

  async_single_request_op(
      basic_connection<Stream> * this_,
      http::request<RequestBody> &req,
      http::response<ResponseBody> &res
      ) : this_(this_), req(req), res(res)
  {
  }

  async_single_request_op(
      basic_connection<Stream> * this_,
      http::request<RequestBody> &&req,
      http::response<ResponseBody> &&res
      ) : this_(this_), req(req), res(res)
  {
  }


  detail::tracker t{this_->ongoing_requests_};
  lock_type lock;
  boost::optional<lock_type> alock;

  boost::optional<http::response_parser<ResponseBody>> hparser;
  asio::coroutine inner_coro;

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code, variant2::variant<std::size_t, lock_type>);

  void resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, variant2::variant<std::size_t, lock_type> res_ = std::size_t{})
  {
    reenter(this)
    {
      yield asem::async_lock(this_->write_mtx_, std::move(self));
      if (ec)
        return;

      lock = std::move(variant2::get<1>(res_));

      // disconnect first
      if (!this_->is_open() && this_->keep_alive_set_.timeout < std::chrono::system_clock::now())
      {
        yield asem::async_lock(this_->read_mtx_, std::move(self));
        if (ec)
          return;
        alock.emplace(std::move(variant2::get<1>(res_)));
        // if the close goes wrong - so what, unless it's still open
        yield detail::async_close_impl(this_->next_layer_, std::move(self));
        ec.clear();
      }

      if (!this_->is_open())
      {
       retry:
        if (!alock)
        {
          yield asem::async_lock(this_->write_mtx_, std::move(self));
          if (ec)
            alock.emplace(std::move(variant2::get<1>(res_)));
        }
        detail::async_connect_impl(this_->next_layer_, this_->endpoint_, std::move(self));
      }

      alock.reset();

      req.set(beast::http::field::host, this_->host_);
      req.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);
      req.prepare_payload();

      yield beast::http::async_write(this_->next_layer_, req, std::move(self));

      if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
        goto retry ;
      else  if (ec)
        return ;

      yield asem::async_lock(this_->read_mtx_, std::move(self));
      if (ec)
        return ;
      lock = std::move(variant2::get<1>(res_));

      if (req.base().method() == beast::http::verb::head)
      {
        hparser.emplace(std::move(res.base()));
        yield
        {
          auto & hp = *hparser;
          beast::http::async_read_header(this_->next_layer_,  this_->buffer_, hp, std::move(self));
        }

        res = std::move(hparser->get());
      }
      else
        yield beast::http::async_read(this_->next_layer_,  this_->buffer_, res, std::move(self));

      if (ec)
        return ;

      // should_close
      if (interpret_keep_alive_response(this_->keep_alive_set_, res, ec))
      {
        yield asem::async_lock(this_->write_mtx_, std::move(self));
        alock.emplace(std::move(variant2::get<1>(res_)));
        if (!ec)
        {
          yield detail::async_close_impl(this_->next_layer_, std::move(self));
        }

        ec = ec_;
      }
    }
  }
};

template<typename Stream>
template<typename RequestBody, typename ResponseBody,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
basic_connection<Stream>::async_single_request(
                     http::request<RequestBody> &req,
                     http::response<ResponseBody> & res,
                     CompletionToken && completion_token)
{
  return detail::co_run<async_single_request_op<RequestBody, ResponseBody>>(
      std::forward<CompletionToken>(completion_token), this, req, res);
}


template<typename Stream>
template<typename RequestBody>
auto basic_connection<Stream>::request(
            beast::http::verb method,
            urls::url_view path,
            RequestBody && body,
            request_settings req,
            system::error_code & ec) -> response
{
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  constexpr auto is_secure = detail::has_ssl_v<Stream>;
  const auto alloc = req.get_allocator();
  using response_type = response ;
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
    using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
    auto cc = req.jar->get<allocator_type>(host(), is_secure, path.encoded_target(),  &memres);
    if (!cc.empty())
      req.fields.set(http::field::cookie, cc);
  }

  beast::http::request<body_type, http::fields> hreq{method, path.encoded_target(), 11,
                                                    body_traits::make_body(std::forward<RequestBody>(body), ec),
                                                    std::move(req.fields)};

  using flat_buffer = beast::basic_flat_buffer<boost::container::pmr::polymorphic_allocator<char>>;

  hreq.prepare_payload();
  using res_body = beast::http::basic_dynamic_body<flat_buffer>;
  beast::http::response<res_body, http::fields> rres{beast::http::response_header<http::fields>{alloc},
                                                     flat_buffer{alloc}};

  single_request(hreq, rres, ec);

  using response_type = response;

  auto rc = rres.base().result();
  while (!ec &&
         (req.opts.redirect >= redirect_mode::endpoint)
         && ((rc == http::status::moved_permanently)
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

    const auto url = interpret_location(hreq.target(), loc_itr->value());
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

    if (--req.opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      break ;
    }
    res.history.emplace_back(std::move(rres.base()));

    hreq.base().target(url->encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = req.jar->get(host(), is_secure, url->encoded_path(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
    }


    single_request(hreq, rres, ec);
    rc = rres.base().result();
  }

  res.headers = std::move(rres.base());
  res.buffer = std::move(rres.body());

  return res;
}

template<typename Stream>
template<typename RequestBody>
struct basic_connection<Stream>::async_request_op : asio::coroutine
{
  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return this_->get_executor(); }

  basic_connection<Stream> * this_;
  constexpr static auto is_secure = detail::has_ssl_v<Stream>;

  beast::http::verb method;

  cookie_jar * jar = nullptr;
  struct request_options opts;
  urls::url_view path;
  core::string_view default_mime_type;

  system::error_code ec_;
  using body_type = RequestBody;
  using response_type = response ;
  using flat_buffer = beast::basic_flat_buffer<boost::container::pmr::polymorphic_allocator<char>>;
  using res_body = beast::http::basic_dynamic_body<flat_buffer>;

  request_settings req;

  response_type res;

  beast::http::request<body_type, http::fields> hreq;
  beast::http::response<res_body, http::fields> rres{beast::http::response_header<http::fields>{req.get_allocator()},
                                                     flat_buffer{req.get_allocator()}};

  template<typename RequestBody_>
  async_request_op(basic_connection<Stream> * this_,
                   beast::http::verb v,
                   urls::url_view path,
                   RequestBody_ && body,
                   request_settings req)
      : this_(this_), method(v), jar(req.jar), opts(req.opts), path(path),
        default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
        req(std::move(req)),
        res{this->req.get_allocator()},
        hreq{v, path.encoded_target(), 11,
             request_body_traits<std::decay_t<RequestBody_>>::make_body(std::forward<RequestBody_>(body), ec_),
             std::move(this->req.fields)}
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
      auto itr = hreq.base().find(http::field::content_type);
      if (itr == hreq.base().end() && !default_mime_type.empty()) {
        if (!default_mime_type.empty())
          hreq.base().set(http::field::content_type, default_mime_type);
      }
    }
    if (jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};

      auto cc = jar->get(this_->host(), is_secure, path.encoded_target(), alloc2);
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
    const auto redirect_mode = (std::min)(supported_redirect_mode(), opts.redirect);
    const auto url = interpret_location(hreq.target(), loc_itr->value());
    if (url.has_error())
    {
      ec = url.error();
      return;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority()
        && this_->host_ == url->encoded_host()
        && !same_endpoint_on_host(*url, this_->endpoint()))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      return;
    }

    if (--opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      return;
    }
    if (jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};

      auto cc = jar->get(this_->host(), is_secure, url->encoded_path(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
      else
        hreq.base().erase(http::field::cookie);
    }

    res.history.emplace_back(std::move(rres.base()));
    hreq.base().target(url->encoded_path());

  }

  using completion_signature_type = void(system::error_code, response);
  using step_signature_type       = void(system::error_code);

  auto resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec) -> response_type &
  {
    auto rc = rres.base().result();

    reenter(this)
    {
      ec = ec_;
      if (!ec)
      {
        prepare_initial_head_request(ec);
      }
      if (ec)
      {
        ec_ = ec;
        yield asio::post(std::move(self));
        ec = ec_;
        break;
      }

      yield this_->async_single_request(hreq, rres, std::move(self));

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
        yield this_->async_single_request(hreq, rres, std::move(self));
      }

      res.buffer = std::move(rres.body());
    complete:
      res.headers = std::move(rres.base());
    }
    return res;
  }
};

#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::empty_body>;
extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::file_body>;
extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::string_body>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::empty_body>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::file_body>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::string_body>;

#endif


template<typename Stream>
template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
basic_connection<Stream>::async_request(beast::http::verb method,
                                        urls::url_view path,
                                        RequestBody && body,
                                        request_settings req,
                                        CompletionToken && completion_token)
{
  using op_t = async_request_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>;
  return detail::co_run<op_t>(
      std::forward<CompletionToken>(completion_token),
      this, method, path,
      std::forward<RequestBody>(body),
      std::move(req)
  );
}


template<typename Stream>
template<typename Body>
void basic_connection<Stream>::write_impl(http::request<Body> & req,
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
    unsigned char buf[4096];
    boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
    using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
    auto cc = jar->get<allocator_type>(host(), is_secure, req.target(),  &memres);
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

    req.base().target(url->encoded_path());
    if (jar)
    {
      unsigned char buf[4096];
      container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = jar->get(host(), is_secure, url->encoded_path(), alloc2);
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

  if ((path.has_port() && (get_port(path) != endpoint_.port()))
  && (path.has_authority() && (path.encoded_host() != host()))
  && (path.has_scheme() && (path.host() != (detail::has_ssl_v<Stream> ? "https" : "http"))))
    BOOST_REQUESTS_ASSIGN_EC(ec, error::wrong_host)


  if (!is_secure && req.opts.enforce_tls)
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
    if (!is_secure && req.opts.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::insecure, &loc);
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
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
      auto cc = req.jar->get<allocator_type>(host, is_secure, path,  &memres);
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

        if ((path.has_port() && (get_port(path) != this_->endpoint_.port()))
            && (path.has_authority() && (path.encoded_host() != this_->host()))
            && (path.has_scheme() && (path.host() != (detail::has_ssl_v<Stream> ? "https" : "http"))))
          BOOST_REQUESTS_ASSIGN_EC(ec_, error::wrong_host)
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

        req.base().target(url->encoded_path());
        if (jar)
        {
          unsigned char buf_[4096];
          container::pmr::monotonic_buffer_resource memres{buf_, sizeof(buf)};
          container::pmr::polymorphic_allocator<char> alloc2{&memres};
          auto cc = jar->get(this_->host(), detail::has_ssl_v<Stream>, url->encoded_path(), alloc2);
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
template<typename RequestBody,
          typename CompletionToken>
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
template<typename RequestBody,
          typename CompletionToken>
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
template<typename RequestBody,
         typename CompletionToken>
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
template<typename RequestBody,
         typename CompletionToken>
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
template<typename RequestBody,
          typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, typename basic_connection<Stream>::stream))
basic_connection<Stream>::async_ropen(http::request<http::string_body> & req,
                                      request_options opt,
                                      cookie_jar * jar,
                                      CompletionToken && completion_token)
{
  return detail::co_run<async_ropen_empty_op>(std::forward<CompletionToken>(completion_token), this, &req, std::move(opt), jar);
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

