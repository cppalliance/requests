//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_HPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/ssl.hpp>
#include <boost/requests/fields/location.hpp>

#include <boost/asem/lock_guard.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/yield.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/core/exchange.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/requests/detail/state_machine.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/url/grammar/ci_string.hpp>

/*

 The statemachine for processing a request

 @startuml
 [*] --> WriteInit : Acquire write lock
 WriteInit --> WriteDone : Socket open & usable (do write)

 WriteInit --> Connect : Socket Closed
 Connect --> WriteDone : do_write

 WriteInit  --> Disconnect: Expired (keep alive is zero)
 Disconnect --> Connect

 WriteDone --> Connect: Disconnnect error
 WriteDone --> ReadReady: Acquire read lock
 ReadReady --> ReadDone : do read

 ReadDone --> [*] : Keep-Alive
 ReadDone --> CloseAfter : no Keep-Alive
 CloseAfter --> [*]

 @enduml

 */

namespace boost {
namespace requests {
namespace detail {

struct tracker
{
  std::atomic<std::size_t> *cnt = nullptr;
  tracker() = default;
  tracker(std::atomic<std::size_t> &cnt) : cnt(&cnt) {++cnt;}
  ~tracker()
  {
    if (cnt) --(*cnt);
  }

  tracker(const tracker &) = delete;
  tracker(tracker && lhs) noexcept : cnt(boost::exchange(lhs.cnt, nullptr))
  {

  }
  tracker& operator=(tracker && lhs) noexcept
  {
    std::swap(cnt, lhs.cnt);
    return *this;
  }

};

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
template<typename RequestBody, typename RequestAllocator,
          typename ResponseBody, typename ResponseAllocator>
void basic_connection<Stream>::single_request(
    beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &req,
    beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res,
    system::error_code & ec)
{
  ongoing_requests_++;
  detail::tracker t{ongoing_requests_};
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  std::chrono::system_clock::time_point now;
  lock_type lock, alock;

  // INIT
  {
    checked_call(lock = asem::lock, write_mtx_);
    goto write_init;
  }

  state(write_init)
  {
    if (is_open() && keep_alive_set_.timeout < std::chrono::system_clock::now())
      goto disconnect;
    else if (!is_open())
      goto connect;
    else
      goto do_write;
  }

  state(disconnect)
  {
    checked_call(alock = asem::lock, read_mtx_);
    detail::close_impl(next_layer_, ec);
    ec.clear();
    goto connect ;
  }

  state(connect)
  {
    if (read_mtx_.try_lock())
      alock = lock_type(read_mtx_, std::adopt_lock);

    checked_call(detail::connect_impl, next_layer_, endpoint_);
    alock = {};

    goto do_write ;
  }

  state(do_write)
  {
    req.set(beast::http::field::host, host_);
    req.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    beast::http::write(next_layer_, req, ec);

    if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
      goto connect ;

    checked_call(lock = asem::lock, read_mtx_);
    goto read_done ;
  }

  state(read_done)
  {
    if (req.base().method() == beast::http::verb::head)
    {
      beast::http::response_parser<ResponseBody, ResponseAllocator> ps{std::move(res.base())};
      beast::http::read_header(next_layer_, buffer_, ps, ec);
      res = std::move(ps.get());
    }
    else
      beast::http::read(next_layer_, buffer_, res, ec);


    now = std::chrono::system_clock::now();
    const auto conn_itr = res.find(beast::http::field::connection);
    if (ec)
    {
      keep_alive_set_.timeout = std::chrono::system_clock::time_point::min();
      keep_alive_set_.max = 0ull;
      goto close_after;
    }

    if (conn_itr == res.end())
      return ;

    if (urls::grammar::ci_is_equal(conn_itr->value(), "close"))
      goto close_after;

    if (!urls::grammar::ci_is_equal(conn_itr->value(), "keep-alive"))
      ec = asio::error::invalid_argument;
  }

  state(close_after)
  {
    boost::system::error_code ec_;
    auto lock = asem::lock(write_mtx_, ec_);
    if (!ec_)
      detail::close_impl(next_layer_, ec_);
    return;
  }

  const auto kl_itr = res.find(beast::http::field::keep_alive);
  if (kl_itr == res.end())
    keep_alive_set_ = keep_alive{}; // set to max
  else
  {

    auto rr = parse_keep_alive_field(kl_itr->value(), now);
    if (rr.has_error())
      ec = rr.error();
    else
      keep_alive_set_ = *rr;

    if (keep_alive_set_.timeout < now)
      goto close_after;
  }
}

template<typename Stream>
struct basic_connection<Stream>::async_connect_op
{
  basic_connection<Stream> * this_;
  endpoint_type ep;

  using mutex_type = detail::basic_mutex<executor_type>;

  template<typename Self>
  void operator()(Self && self)
  {
    using namespace asio::experimental;

    make_parallel_group(
        asem::async_lock(this_->write_mtx_, asio::deferred),
        asem::async_lock(this_->read_mtx_, asio::deferred))
          .async_wait(wait_for_all(), std::move(self));
  }

  template<typename Self>
  void operator()(Self && self,
                  std::array<std::size_t, 2u>,
                  system::error_code ec1,
                  asem::lock_guard<mutex_type> lock1,
                  system::error_code ec2,
                  asem::lock_guard<mutex_type> lock2)
  {
    if (ec1 || ec2)
      self.complete(ec1 ? ec1 : ec2);
    else
      beast::get_lowest_layer(this_->next_layer_).async_connect(
          ep,
          asio::append(std::move(self), std::move(lock1), std::move(lock2), detail::get_ssl_layer(this_->next_layer_)));
  }

  template<typename Self, typename Stream_>
  void operator()(Self && self,
                  system::error_code ec,
                  asem::lock_guard<mutex_type> lock1,
                  asem::lock_guard<mutex_type> lock2,
                  Stream_ * stream)
  {
    if (ec)
      self.complete(ec);
    else
      stream->async_handshake(asio::ssl::stream_base::client,
                              asio::append(std::move(self), std::move(lock1), std::move(lock2), nullptr));
  }


  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec,
                  asem::lock_guard<mutex_type> lock1,
                  asem::lock_guard<mutex_type> lock2,
                  std::nullptr_t)
  {
    self.complete(ec);
  }

};

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code))
basic_connection<Stream>::async_connect(endpoint_type ep, CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_connect_op{this, ep},
      completion_token,
      next_layer_
      );
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
template<typename RequestBody, typename RequestAllocator,
         typename ResponseBody, typename ResponseAllocator>
struct basic_connection<Stream>::async_single_request_op : boost::asio::coroutine
{
  basic_connection<Stream> * this_;
  beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &req;
  beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res;

  detail::tracker t{this_->ongoing_requests_};
  using lock_type = asem::lock_guard<detail::basic_mutex<typename Stream::executor_type>>;
  std::chrono::system_clock::time_point now;
  lock_type lock, alock;

  std::shared_ptr<beast::http::response_parser<ResponseBody, ResponseAllocator>> hparser;

  template<typename Self>
  void handshake_impl(Self && self, std::true_type /* is_ssl */)
  {
    return detail::get_ssl_layer(this_->next_layer_)
        ->async_handshake(asio::ssl::stream_base::client, std::move(self));
  }

  template<typename Self>
  void handshake_impl(Self && self, std::false_type /* is_ssl */)
  {
    return asio::post(self.get_executor(), std::move(self));
  }


  template<typename Self>
  void shutdown_impl(Self && self, std::true_type /* is_ssl */)
  {
    static thread_local system::error_code ec_;
    return detail::get_ssl_layer(this_->next_layer_)
        ->async_shutdown(asio::redirect_error(std::move(self), ec_));
  }

  template<typename Self>
  void shutdown_impl(Self && self, std::false_type /* is_ssl */)
  {
    return asio::post(self.get_executor(), std::move(self));
  }

  template<typename Self>
  void complete(Self && self, error_code ec, lock_type lock_ = {})
  {
    lock_ = {};
    alock = {};
    lock = {};
    t = detail::tracker();
    self.complete(ec);
  }

  template<typename Self>
  void operator()(Self && self, system::error_code ec= {}, lock_type lock_ = {})
  {
    if (ec)
      return complete(std::move(self), ec, std::move(lock_));

    reenter(this)
    {
      // INIT
      {
        yield asem::async_lock(this_->write_mtx_, std::move(self));
        lock = std::move(lock_);
        goto write_init;
      }

      state(write_init)
      {
        if (this_->is_open() && this_->keep_alive_set_.timeout < std::chrono::system_clock::now())
          goto disconnect;
        else if (!this_->is_open())
        {
          yield asem::async_lock(this_->read_mtx_, std::move(self));
          alock = std::move(lock_);
          goto connect;
        }
        else
          goto do_write;
      }

      state(disconnect)
      {
        yield asem::async_lock(this_->read_mtx_, std::move(self));
        alock = std::move(lock_);
        detail::close_impl(this_->next_layer_, ec);
        ec.clear();
        goto connect ;
      }

      state(connect)
      {
        yield beast::get_lowest_layer(this_->next_layer_).async_connect(this_->endpoint_, std::move(self));
        if (detail::has_ssl_v<Stream>)
        {
          yield handshake_impl(std::move(self), detail::has_ssl<Stream>{});
        }
        alock = {};
        goto do_write ;
      }

      state(do_write)
      {
        req.set(beast::http::field::host, this_->host_);
        req.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

        yield beast::http::async_write(this_->next_layer_, req, detail::drop_size())(std::move(self));

        if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
          goto connect ;

        yield asem::async_lock(this_->read_mtx_, std::move(self));
        lock = std::move(lock_);
        goto read_done ;
      }

      state(read_done)
      {
        if (req.base().method() == beast::http::verb::head)
        {
          hparser =
              std::allocate_shared<beast::http::response_parser<ResponseBody, ResponseAllocator>>(
                  self.get_allocator(), std::move(res.base()));

          yield beast::http::async_read_header(this_->next_layer_, this_->buffer_, *hparser,
                                               detail::drop_size())(std::move(self));
          res = std::move(hparser->get());
          hparser.reset();
        }
        else
          yield beast::http::async_read(this_->next_layer_, this_->buffer_, res, detail::drop_size())(std::move(self));

        now = std::chrono::system_clock::now();
        const auto conn_itr = res.find(beast::http::field::connection);
        if (ec)
        {
          this_->keep_alive_set_.timeout = std::chrono::system_clock::time_point::min();
          this_->keep_alive_set_.max = 0ull;
          goto close_after;
        }

        if (conn_itr == res.end())
          return complete(std::move(self), ec, std::move(lock_));

        if (urls::grammar::ci_is_equal(conn_itr->value(), "close"))
          goto close_after;

        if (!urls::grammar::ci_is_equal(conn_itr->value(), "keep-alive"))
          return complete(std::move(self), asio::error::invalid_argument, std::move(lock_));

        goto complete ;
      }

      state(close_after)
      {

        yield asem::async_lock(this_->write_mtx_, std::move(self));
        alock = std::move(lock_);
        if (detail::has_ssl_v<Stream>)
        {
          yield shutdown_impl(std::move(self), detail::has_ssl<Stream>{});
        }
        boost::system::error_code ec_;
        beast::get_lowest_layer(this_->next_layer_).close(ec_);
        return complete(std::move(self), ec, std::move(lock_));
      }
      state(complete)
      {
        const auto kl_itr = res.find(beast::http::field::keep_alive);
        if (kl_itr == res.end())
          this_->keep_alive_set_ = keep_alive{}; // set to max
        else
        {

          auto rr = parse_keep_alive_field(kl_itr->value(), now);
          if (rr.has_error())
            ec = rr.error();
          else
            this_->keep_alive_set_ = *rr;

          if (this_->keep_alive_set_.timeout < now)
            goto close_after;
        }
        return complete(std::move(self), ec, std::move(lock_));
      }
    }
  }
};


template<typename Stream>
template<typename RequestBody, typename RequestAllocator,
          typename ResponseBody, typename ResponseAllocator,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
basic_connection<Stream>::async_single_request(
                     beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &req,
                     beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res,
                     CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_single_request_op<RequestBody, RequestAllocator, ResponseBody, ResponseAllocator>{{}, this, req, res},
      completion_token,
      next_layer_
  );
}


template<typename Stream>
template<typename RequestBody>
auto basic_connection<Stream>::request(
            beast::http::verb method,
            urls::pct_string_view path,
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
    auto cc = req.jar->get<allocator_type>(host(), is_secure, path,  &memres);
    if (!cc.empty())
      req.fields.set(http::field::cookie, cc);
  }

  beast::http::request<body_type, http::fields> hreq{method, path, 11,
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

  res.header = std::move(rres.base());
  res.buffer = std::move(rres.body());

  return res;
}

template<typename Stream>
auto basic_connection<Stream>::download(
            urls::pct_string_view path,
            request_settings req,
            const filesystem::path & download_path,
            system::error_code & ec) -> response
{
  using response_type = response ;

  const auto alloc = req.get_allocator();
  constexpr auto is_secure = detail::has_ssl_v<Stream>;

  response_type res{alloc};

  if (!is_secure && req.opts.enforce_tls)
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    ec.assign(error::insecure, &loc);
    return res;
  }

  beast::http::request<beast::http::empty_body, http::fields> hreq{beast::http::verb::head, path, 11,
                                                                  beast::http::empty_body::value_type{},
                                                                  std::move(req.fields)};

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
  beast::http::response<beast::http::empty_body, http::fields> hres{beast::http::response_header<http::fields>{alloc}};
  hreq.prepare_payload();

  if (req.jar)
  {
    unsigned char buf[4096];
    boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
    container::pmr::polymorphic_allocator<char> alloc2{&memres};

    auto cc = req.jar->get(host(), is_secure, path, alloc2);
    if (!cc.empty())
      hreq.base().set(http::field::cookie, cc);
    else
      hreq.base().erase(http::field::cookie);
  }

  single_request(hreq, hres, ec);
  using response_type = response;


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
    const auto url = interpret_location(hreq.target(), loc_itr->value());
    if (url.has_error())
    {
      ec = url.error();
      res.header = std::move(hres.base());
      return res;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority()
        && host_ == url->encoded_host()
        && !same_endpoint_on_host(*url, endpoint()))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      res.header = std::move(hres.base());
      return res;
    }
    if (--req.opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      res.header = std::move(hres.base());
      return res;
    }
    res.history.emplace_back(std::move(hres.base()));
    hreq.base().target(url->encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
            auto cc = req.jar->get(host(), is_secure, url->encoded_path(), alloc2);
      if (!cc.empty())
        hreq.base().set(http::field::cookie, cc);
      else
          hreq.base().erase(http::field::cookie);
    }

    single_request(hreq, hres, ec);

    rc = hres.base().result();

  }
  hreq.method(beast::http::verb::get);
  beast::http::response<beast::http::file_body, http::fields> fres{beast::http::response_header<http::fields>{alloc}};

  auto str = download_path.string();
  if (!ec)
    fres.body().open(str.c_str(), beast::file_mode::write, ec);
  if (!ec)
    single_request(hreq, fres, ec);


  res.header = std::move(fres.base());

  return res;
}

template<typename Stream>
template<typename RequestBody>
struct basic_connection<Stream>::async_request_op : asio::coroutine
{

  basic_connection<Stream> * this_;
  constexpr static auto is_secure = detail::has_ssl_v<Stream>;

  beast::http::verb method;

  cookie_jar * jar = nullptr;
  struct request_options opts;
  core::string_view path;
  core::string_view default_mime_type;

  system::error_code ec_;
  using body_type = RequestBody;

  typename body_type::value_type body;
  request_settings req;
  struct state_t
  {
    boost::container::pmr::polymorphic_allocator<char> alloc;


    //using body_traits = request_body_traits<std::decay_t<RequestBody>>;
    using response_type = response ;
    response_type res;

    beast::http::request<body_type, http::fields> hreq;
    using flat_buffer = beast::basic_flat_buffer<boost::container::pmr::polymorphic_allocator<char>>;
    using res_body = beast::http::basic_dynamic_body<flat_buffer>;
    beast::http::response<res_body, http::fields> rres{beast::http::response_header<http::fields>{alloc},
                                                       flat_buffer{alloc}};

    template<typename RequestBody_>
    state_t(beast::http::verb v,
            urls::pct_string_view path,
            RequestBody_ && body,
            request_settings req)
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
  async_request_op(basic_connection<Stream> * this_,
                   beast::http::verb v,
                   urls::pct_string_view path,
                   RequestBody_ && body,
                   request_settings req)
      : this_(this_), method(v), jar(req.jar), opts(req.opts), path(path),
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
    if (jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};

      auto cc = jar->get(this_->host(), is_secure, path, alloc2);
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
    const auto redirect_mode = (std::min)(supported_redirect_mode(), opts.redirect);
    const auto url = interpret_location(state_->hreq.target(), loc_itr->value());
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
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }

    state_->res.history.emplace_back(std::move(state_->rres.base()));
    state_->hreq.base().target(url->encoded_path());

  }
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
                                               method, path, std::move(body), std::move(req));
        prepare_initial_head_request(ec);
      }
      if (ec)
      {
        yield asio::post(std::move(self));
        return self.complete(ec, std::move(state_->res));
      }

      yield this_->async_single_request(state_->hreq, state_->rres, std::move(self));

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
        yield this_->async_single_request(state_->hreq, state_->rres, std::move(self));
      }

      state_->res.buffer = std::move(state_->rres.body());
    complete:
      state_->res.header = std::move(state_->rres.base());
      self.complete(ec, std::move(state_->res));
    }
  }
};

#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::empty_body>;
extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::string_body>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::empty_body>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::string_body>;

#endif


template<typename Stream>
template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
basic_connection<Stream>::async_request(beast::http::verb method,
                                        urls::pct_string_view path,
                                        RequestBody && body,
                                        request_settings req,
                                        CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, response)>(
      async_request_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type>{
                                                  this, method, path,
                                                  std::forward<RequestBody>(body),
                                                  std::move(req)},
      completion_token,
      next_layer_
  );
}

template<typename Stream>
struct basic_connection<Stream>::async_download_op : asio::coroutine
{
  using response_type = response ;

  basic_connection<Stream>* this_;
  constexpr static auto is_secure = detail::has_ssl_v<Stream>;
  cookie_jar * jar;
  struct request_options opts;
  core::string_view path;
  core::string_view default_mime_type;
  filesystem::path download_path;

  request_settings req;

  struct state_t
  {
    boost::container::pmr::polymorphic_allocator<char> alloc;
    response_type res{alloc};
    beast::http::request<beast::http::empty_body, http::fields> hreq;
    beast::http::response<beast::http::empty_body, http::fields> hres{beast::http::response_header<http::fields>{alloc}};
    beast::http::response<beast::http::file_body, http::fields>  fres{beast::http::response_header<http::fields>{alloc}};


    state_t(urls::pct_string_view path,
            request_settings req)
          :  alloc(req.get_allocator()),
            hreq{beast::http::verb::head, path, 11,
                 beast::http::empty_body::value_type{},
                 std::move(req.fields)}
    {
    }

  };


  std::shared_ptr<state_t> state_;

  template<typename ... Args>
  async_download_op(basic_connection<Stream> * this_, urls::pct_string_view path,
                    const filesystem::path & download_path,
                    request_settings req)
      : this_(this_), jar(req.jar), opts(req.opts), path(path),
        download_path(download_path)
  {
  }

  void prepare_initial_request(system::error_code &ec)
  {

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

    if (jar)
     {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};

      auto cc = jar->get(this_->host(), is_secure, path, alloc2);
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
    const auto url = interpret_location(state_->hreq.target(), loc_itr->value());
    if (url.has_error())
    {
      ec = url.error();
      state_->res.header = std::move(state_->hres);
      return ;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority()
        && this_->host_ == url->encoded_host()
        && !same_endpoint_on_host(*url, this_->endpoint()))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      state_->res.header = std::move(state_->hres);
      return ;
    }

    if (--opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      state_->res.header = std::move(state_->hres);
      return ;
    }
    state_->res.history.emplace_back(std::move(state_->hres.base()));

    state_->hreq.base().target(url->encoded_path());
    if (jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      container::pmr::polymorphic_allocator<char> alloc2{&memres};
      auto cc = jar->get(this_->host(), is_secure, url->encoded_path(), alloc2);
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
      state_ = std::allocate_shared<state_t>(self.get_allocator(), path, std::move(req));
      prepare_initial_request(ec);
      if (ec)
      {
        yield asio::post(asio::append(std::move(self), ec));
        return self.complete(ec, std::move(state_->res));
      }
      yield this_->async_single_request(state_->hreq, state_->hres, std::move(self));

      if (ec)
        return self.complete(ec, std::move(state_->res));

      while ((opts.redirect >= redirect_mode::endpoint) &&
            ((rc == http::status::moved_permanently) || (rc == http::status::found) ||
            (rc == http::status::temporary_redirect) || (rc == http::status::permanent_redirect)))
      {
        handle_redirect(ec);
        if (ec)
          goto complete;
        yield this_->async_single_request(state_->hreq, state_->hres, std::move(self));
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
        yield this_->async_single_request(state_->hreq, state_->fres, std::move(self));
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

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
basic_connection<Stream>::async_download(urls::pct_string_view path,
                                         request_settings req,
                                         filesystem::path download_path,
                                         CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken,
                             void(system::error_code, response)>(
      async_download_op{this, path, std::move(download_path), std::move(req)},
      completion_token,
      next_layer_
  );
}

#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template auto basic_connection<asio::ip::tcp::socket>::                   download(urls::pct_string_view, request_settings, const filesystem::path &, system::error_code &) -> response;
extern template auto basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::download(urls::pct_string_view, request_settings, const filesystem::path &, system::error_code &) -> response;

#endif


}
}

#include <boost/requests/detail/undef.hpp>
#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP
