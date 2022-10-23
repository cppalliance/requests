//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_HPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/lock.hpp>
#include <boost/requests/detail/ssl.hpp>

#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/yield.hpp>
#include <boost/asem/lock_guard.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/requests/detail/state_machine.hpp>
#include <boost/requests/request.hpp>
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
  std::size_t *cnt = nullptr;
  tracker(std::size_t &cnt) : cnt(&cnt) {++cnt;}
  ~tracker() { if (cnt) --(*cnt);}

  tracker(const tracker &) = delete;
  tracker(tracker && lhs)
  {
    std::swap(cnt, lhs.cnt);
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
  using lock_type = detail::lock<asem::basic_mutex<boost::asem::st, executor_type>>;

  lock_type wlock{write_mtx_, ec};
  if (ec)
    return;

  lock_type rlock{read_mtx_, ec};
  if (ec)
    return;

  detail::connect_impl(next_layer_, endpoint_ = ep, ec);
}


template<typename Stream>
void basic_connection<Stream>::close(system::error_code & ec)
{
  using lock_type = detail::lock<asem::basic_mutex<boost::asem::st, executor_type>>;

  lock_type wlock{write_mtx_, ec};
  if (ec)
    return;

  lock_type rlock{read_mtx_, ec};
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
  using lock_type = detail::lock<asem::basic_mutex<boost::asem::st, executor_type>>;
  std::chrono::system_clock::time_point now;
  lock_type lock, alock;

  // INIT
  {
    checked_call(lock = lock_type, write_mtx_);
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
    checked_call(alock = lock_type, read_mtx_);
    detail::close_impl(next_layer_, ec);
    ec.clear();
    goto connect ;
  }

  state(connect)
  {
    if (!alock)
      checked_call(alock = lock_type, read_mtx_);

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

    checked_call(lock = lock_type, read_mtx_);
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
    {
      ec = asio::error::invalid_argument;
      return ;
    }
  }

  state(close_after)
  {
    boost::system::error_code ec_;
    lock_type rlock{write_mtx_, ec_};
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

  using mutex_type = asem::basic_mutex<boost::asem::st, executor_type>;

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
  using mutex_type = asem::basic_mutex<boost::asem::st, executor_type>;
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
  detail::tracker t{++this_->ongoing_requests_};
  using lock_type = asem::lock_guard<asem::basic_mutex<boost::asem::st, typename Stream::executor_type>>;
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
  void operator()(Self && self, system::error_code ec= {}, lock_type lock_ = {})
  {
    if (ec)
      return self.complete(ec);

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

          yield {
            auto p = hparser;
            beast::http::async_read_header(this_->next_layer_, this_->buffer_, *p,
                                           detail::drop_size())(std::move(self));
          };
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
          return self.complete(ec);

        if (urls::grammar::ci_is_equal(conn_itr->value(), "close"))
          goto close_after;

        if (!urls::grammar::ci_is_equal(conn_itr->value(), "keep-alive"))
        {
          ec = asio::error::invalid_argument;
          return self.complete(ec);
        }
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
        return self.complete(ec);
      }

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

      return self.complete(ec);
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
template<typename RequestBody, typename Allocator>
auto basic_connection<Stream>::request(
            beast::http::verb method,
            urls::pct_string_view path,
            RequestBody && body,
            basic_request<Allocator> req,
            system::error_code & ec) -> basic_response<Allocator>
{
  using body_traits = request_body_traits<std::decay_t<RequestBody>>;
  using body_type = typename body_traits::body_type;
  using fields_type = beast::http::basic_fields<Allocator>;
  constexpr auto is_secure = detail::has_ssl_v<Stream>;
  const auto alloc = req.get_allocator();
  using response_type = basic_response<Allocator> ;
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
    auto cc = req.jar->get(host(),  &memres, is_secure, path);
    if (!cc.empty())
      req.fields.set(http::field::cookie, cc);
  }

  beast::http::request<body_type, fields_type> hreq{method, path, 11,
                                                    body_traits::make_body(std::forward<RequestBody>(body), ec),
                                                    std::move(req.fields)};
  hreq.prepare_payload();
  using res_body = beast::http::basic_dynamic_body<beast::basic_flat_buffer<Allocator>>;
  beast::http::response<res_body, fields_type> rres{http::response_header<fields_type>{alloc},
                                                    beast::basic_flat_buffer<Allocator>{alloc}};

  single_request(hreq, rres, ec);

  using response_type = basic_response<Allocator>;

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
    const auto & loc = *loc_itr;

    auto url = urls::parse_relative_ref(loc.value());
    if (url.has_error())
      url = urls::parse_uri(loc.value());

    if (url.has_error())
    {
      ec = url.error();
      break;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority() && !same_host(*url, endpoint()))
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
    res.history.emplace_back(std::move(rres.base()), std::move(rres.body()));

    hreq.base().target(url->encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = req.jar->get(host(), &memres, is_secure, url->encoded_path());
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
template<typename Allocator>
auto basic_connection<Stream>::download(
            urls::pct_string_view path,
            basic_request<Allocator> req,
            const filesystem::path & download_path,
            system::error_code & ec) -> basic_response<Allocator>
{
  using fields_type = beast::http::basic_fields<Allocator>;
  using response_type = basic_response<Allocator> ;

  const auto alloc = req.get_allocator();
  constexpr auto is_secure = detail::has_ssl_v<Stream>;

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
    auto cc = req.jar->get(host(),  &memres, is_secure, path);
    if (!cc.empty())
      hreq.base().set(http::field::cookie, cc);
    else
      hreq.base().erase(http::field::cookie);
  }

  single_request(hreq, hres, ec);
  using response_type = basic_response<Allocator>;


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
    const auto & loc = *loc_itr;
    auto url = urls::parse_relative_ref(loc.value());

    if (url.has_error())
      url = urls::parse_uri(loc.value());

    if (url.has_error())
    {
      ec = url.error();
      res.header = std::move(hres.base());
      return res;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority() && !same_host(*url, endpoint()))
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
    res.history.emplace_back(std::move(hres.base()), (typename response_type::body_type::value_type){alloc});
    hreq.base().target(url->encoded_path());
    if (req.jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = req.jar->get(host(), &memres, is_secure, url->encoded_path());
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

template<typename Stream>
template<typename RequestBody, typename Allocator, typename CplAlloc>
struct basic_connection<Stream>::async_request_op : asio::coroutine
{

  basic_connection<Stream> * this_;
  constexpr static auto is_secure = detail::has_ssl_v<Stream>;

  struct state_t
  {
    beast::http::verb method;

    cookie_jar_base * jar = nullptr;
    struct options opts;
    Allocator alloc;

    core::string_view path;
    core::string_view default_mime_type;

    //using body_traits = request_body_traits<std::decay_t<RequestBody>>;
    using body_type = RequestBody;
    using fields_type = beast::http::basic_fields<Allocator>;
    using response_type = basic_response<Allocator> ;
    response_type res;

    system::error_code ec;

    beast::http::request<body_type, fields_type> hreq;
    using res_body = beast::http::basic_dynamic_body<beast::basic_flat_buffer<Allocator>>;
    beast::http::response<res_body, fields_type> rres{http::response_header<fields_type>{alloc},
                                                      beast::basic_flat_buffer<Allocator>{alloc}};

    template<typename RequestBody_>
    state_t(beast::http::verb v,
            urls::pct_string_view path,
            RequestBody_ && body,
            basic_request<Allocator> req)
        :  method(v), jar(req.jar), opts(req.opts), alloc(req.get_allocator()),  path(path),
           default_mime_type(request_body_traits<std::decay_t<RequestBody_>>::default_content_type(body)),
           res{req.get_allocator()},
           hreq{v, path, 11,
               request_body_traits<std::decay_t<RequestBody_>>::make_body(std::forward<RequestBody_>(body), ec),
               std::move(req.fields)}
    {
    }
  };


  using allocator_type = typename std::allocator_traits<CplAlloc>::template rebind_alloc<state_t>;
  std::unique_ptr<state_t, boost::alloc_deleter<state_t, allocator_type> > state_;

  template<typename ... Args>
  async_request_op(allocator_type alloc, basic_connection<Stream> * this_, Args && ... args)
      : this_(this_), state_(boost::allocate_unique<state_t>(alloc, std::forward<Args>(args)...))
  {
  }

  void prepare_initial_head_request()
  {
    if (state_->ec)
      return ;

    if (!is_secure && state_->opts.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      state_->ec = system::error_code(error::insecure, &loc);
      return ;
    }

    {
      auto itr = state_->hreq.base().find(http::field::content_type);
      if (itr == state_->hreq.base().end() && !state_->default_mime_type.empty()) {
        if (!state_->default_mime_type.empty())
          state_->hreq.base().set(http::field::content_type, state_->default_mime_type);
      }
    }
    if (state_->jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = state_->jar->get(this_->host(),  &memres, is_secure, state_->path);
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
    const auto & loc = *loc_itr;

    const auto redirect_mode = (std::min)(supported_redirect_mode(), state_->opts.redirect);
    auto url = urls::parse_relative_ref(loc.value());
    if (url.has_error())
      url = urls::parse_uri(loc.value());

    if (url.has_error())
    {
      ec = url.error();
      return;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority() && !same_host(*url, this_->endpoint()))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      return;
    }

    if (--state_->opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      return;
    }
    if (state_->jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = state_->jar->get(this_->host(), &memres, is_secure, url->encoded_path());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }

    state_->res.history.emplace_back(std::move(state_->rres.base()), std::move(state_->rres.body()));
    state_->hreq.base().target(url->encoded_path());

  }
  template<typename Self>
  void operator()(Self && self, system::error_code ec= {})
  {
    auto rc = state_->rres.base().result();

    reenter(this)
    {
      prepare_initial_head_request();
      if (state_->ec)
      {
        yield asio::post(std::move(self));
        return self.complete(state_->ec, std::move(state_->res));
      }

      yield this_->async_single_request(state_->hreq, state_->rres, std::move(self));

      while (!ec &&
           (state_->opts.redirect >= redirect_mode::endpoint)
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

extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::empty_body,  std::allocator<char>, std::allocator<void>>;
extern template struct basic_connection<asio::ip::tcp::socket>::async_request_op<beast::http::string_body, std::allocator<char>, std::allocator<void>>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::empty_body,  std::allocator<char>, std::allocator<void>>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_request_op<beast::http::string_body, std::allocator<char>, std::allocator<void>>;

#endif


template<typename Stream>
template<typename RequestBody,
          typename Allocator,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, basic_response<Allocator>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        basic_response<Allocator>))
basic_connection<Stream>::async_request(beast::http::verb method,
                                        urls::pct_string_view path,
                                        RequestBody && body,
                                        basic_request<Allocator> req,
                                        CompletionToken && completion_token)
{
  using allocator_type = asio::associated_allocator_t<std::decay_t<CompletionToken>>;
  return asio::async_compose<CompletionToken, void(system::error_code, basic_response<Allocator>)>(
      async_request_op<typename request_body_traits<std::decay_t<RequestBody>>::body_type,
                       Allocator, allocator_type>{asio::get_associated_allocator(completion_token),
                                                  this, method, path,
                                                  std::forward<RequestBody>(body),
                                                  std::move(req)},
      completion_token,
      next_layer_
  );
}

template<typename Stream>
template<typename Allocator, typename CplAlloc>
struct basic_connection<Stream>::async_download_op : asio::coroutine
{
  using fields_type = beast::http::basic_fields<Allocator>;
  using response_type = basic_response<Allocator> ;

  basic_connection<Stream>* this_;
  constexpr static auto is_secure = detail::has_ssl_v<Stream>;

  struct state_t
  {
    cookie_jar_base * jar;
    struct options opts;
    Allocator alloc;

    core::string_view path;
    core::string_view default_mime_type;

    response_type res{alloc};

    beast::http::request<http::empty_body, fields_type> hreq;
    http::response<http::empty_body, fields_type> hres{http::response_header<fields_type>{alloc}};
    http::response<http::file_body, fields_type> fres{http::response_header<fields_type>{alloc}};

    filesystem::path download_path;

    state_t(urls::pct_string_view path,
            const filesystem::path & download_path,
            basic_request<Allocator> req)
          : jar(req.jar), opts(req.opts), alloc(req.get_allocator()), path(path),
            hreq{http::verb::head, path, 11,
                 http::empty_body::value_type{},
                 std::move(req.fields)},
            download_path(download_path)
    {
    }

  };


  using allocator_type = typename std::allocator_traits<CplAlloc>::template rebind_alloc<state_t>;
  std::unique_ptr<state_t, boost::alloc_deleter<state_t, allocator_type> > state_;

  template<typename ... Args>
  async_download_op(allocator_type alloc, basic_connection<Stream> * this_, Args && ... args)
      : this_(this_), state_(boost::allocate_unique<state_t>(alloc, std::forward<Args>(args)...))
  {
  }

  void prepare_initial_request(system::error_code &ec)
  {

    if (!is_secure && state_->opts.enforce_tls)
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      ec.assign(error::insecure, &loc);
      return ;
    }

    // set mime-type
    {
      auto hitr = state_->hreq.base().find(http::field::accept);
      if (hitr == state_->hreq.base().end()) {
        const auto ext = state_->download_path.extension().string();
        const auto &mp = default_mime_type_map();
        auto itr = mp.find(ext);
        if (itr != mp.end())
          state_->hreq.base().set(http::field::accept, itr->second);
      }
    }

    state_->hreq.prepare_payload();

    if (state_->jar) {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = state_->jar->get(this_->host(), &memres, is_secure, state_->path);
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
    const auto &loc = *loc_itr;

    auto url = urls::parse_relative_ref(loc.value());
    if (url.has_error())
      url = urls::parse_uri(loc.value());

    if (url.has_error())
    {
      ec = url.error();
      state_->res.header = std::move(state_->hres);
      return ;
    }
    // we don't need to use should_redirect, bc we're on the same host.
    if (url->has_authority() && !same_host(*url, this_->endpoint()))
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::forbidden_redirect, &sloc);
      state_->res.header = std::move(state_->hres);
      return ;
    }

    if (--state_->opts.max_redirects == 0)
    {
      static constexpr auto sloc((BOOST_CURRENT_LOCATION));
      ec.assign(error::too_many_redirects, &sloc);
      state_->res.header = std::move(state_->hres);
      return ;
    }
    state_->res.history.emplace_back(std::move(state_->hres.base()),
                                     (typename response_type::body_type::value_type){state_->alloc});

    state_->hreq.base().target(url->encoded_path());
    if (state_->jar)
    {
      unsigned char buf[4096];
      boost::container::pmr::monotonic_buffer_resource memres{buf, sizeof(buf)};
      auto cc = state_->jar->get(this_->host(), &memres, is_secure, url->encoded_path());
      if (!cc.empty())
        state_->hreq.base().set(http::field::cookie, cc);
      else
        state_->hreq.base().erase(http::field::cookie);
    }
  }

  template<typename Self>
  void operator()(Self && self, boost::system::error_code ec = {})
  {
    const auto rc = state_->hres.result();

    reenter(this)
    {
      prepare_initial_request(ec);
      if (ec)
      {
        yield asio::post(asio::append(std::move(self), ec));
        return self.complete(ec, std::move(state_->res));
      }
      yield this_->async_single_request(state_->hreq, state_->hres, std::move(self));

      if (ec)
        return self.complete(ec, std::move(state_->res));

      while ((state_->opts.redirect >= redirect_mode::endpoint) &&
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
          auto str = state_->download_path.string();
          state_->hreq.method(beast::http::verb::get);
          state_->fres.body().open(str.c_str(), beast::file_mode::write, ec);
        }
        if (ec)
          goto complete;
        yield this_->async_single_request(state_->hreq, state_->fres, asio::append(std::move(self), 0));
      }
      else complete:
        return self.complete(ec, std::move(state_->res));
    }
  }

  template<typename Self>
  void operator()(Self && self,
                  boost::system::error_code ec, int)
  {
    state_->res.header = std::move(state_->fres.base());
    self.complete(ec, std::move(state_->res));
  }

};


#if !defined(BOOST_REQUESTS_HEADER_ONLY)

extern template struct basic_connection<asio::ip::tcp::socket>::async_download_op<std::allocator<char>, std::allocator<void>>;
extern template struct basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>::async_download_op<std::allocator<char>, std::allocator<void>>;

#endif


template<typename Stream>
template<typename Allocator,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, basic_response<Allocator>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        basic_response<Allocator>))
basic_connection<Stream>::async_download(urls::pct_string_view path,
                                         basic_request<Allocator> req,
                                         const filesystem::path & download_path,
                                         CompletionToken && completion_token)
{
  using allocator_type = asio::associated_allocator_t<std::decay_t<CompletionToken>>;
  return asio::async_compose<CompletionToken,
                             void(system::error_code, basic_response<Allocator>)>(
      async_download_op<Allocator, allocator_type>{asio::get_associated_allocator(completion_token),
                                   this, path, download_path, std::move(req)},
      completion_token,
      next_layer_
  );
}




}
}

#include <boost/requests/detail/undef.hpp>
#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP
