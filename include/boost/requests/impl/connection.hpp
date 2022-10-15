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
#include <boost/asem/lock_guard.hpp>
#include <boost/url/grammar/ci_string.hpp>

#include <boost/requests/detail/define.hpp>
#include <boost/requests/detail/state_machine.hpp>

#include <boost/asio/yield.hpp>

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
namespace {

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
  if (ec)
    return ;

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
    beast::http::write(next_layer_, req,ec);

    if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
      goto connect ;

    checked_call(lock = lock_type, read_mtx_);
    goto read_done ;
  }

  state(read_done)
  {
    beast::http::read(next_layer_, buffer_, res, ec);
    now = std::chrono::system_clock::now();
    const auto conn_itr = res.find(beast::http::field::connection);
    if (ec || conn_itr == res.end())
    {
      keep_alive_set_.timeout = std::chrono::system_clock::time_point::min();
      keep_alive_set_.max = 0ull;
      goto close_after;
    }

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

template<typename Stream>
template<typename RequestBody, typename RequestAllocator,
         typename ResponseBody, typename ResponseAllocator>
struct basic_connection<Stream>::async_single_request_op : boost::asio::coroutine
{
  basic_connection<Stream> * this_;
  beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &request;
  beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & response;
  detail::tracker t{this_->ongoing_requests_};
  std::chrono::system_clock::time_point now;
  system::result<keep_alive> rr;
  typename beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>>::iterator conn_itr, kl_itr;

  using mutex_type = asem::basic_mutex<boost::asem::st, executor_type>;
  using lock_guard = asem::lock_guard<mutex_type>;
  using lock_type = asem::lock_guard<mutex_type>;



  async_init
  {
    asem::async_lock(this_->write_mtx_, async_next(write_init));
  };

  async_state(write_init, system::error_code ec, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);


    if (this_->is_open() && this_->keep_alive_set_.timeout < std::chrono::system_clock::now())
      return asem::async_lock(this_->read_mtx_, async_next(disconnect, std::move(write_lock)));
    else if (!this_->is_open())
      return asem::async_lock(this_->read_mtx_, async_next(connect, std::move(write_lock)));
    else
      async_goto(do_write, {}, {}, std::move(write_lock));
  }

  template<typename next_tag, typename Self>
  void do_disconnect(Self && self, system::error_code ec, lock_type read_lock, lock_type write_lock,
                     std::true_type)
  {
    detail::get_ssl_layer(this_->next_layer_)
        ->async_shutdown(
            asio::deferred(
                [&str = beast::get_lowest_layer(this_->next_layer_)](system::error_code ec)
                {
                  str.close(ec);
                  return asio::deferred.values(ec);
                }))(async_next(next, std::move(read_lock), std::move(write_lock)));
  }

  template<typename next_tag, typename Self>
  void do_disconnect(Self && self, system::error_code ec, lock_type read_lock, lock_type write_lock,
                     std::false_type)
  {
    beast::get_lowest_layer(this_->next_layer_).close(ec);
    async_goto(next, ec, std::move(read_lock), std::move(write_lock));
  }

  async_state(disconnect, system::error_code ec, lock_type read_lock, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);
    do_disconnect<connect_tag>(std::move(self), ec, std::move(read_lock), std::move(write_lock),
                               detail::has_ssl<next_layer_type>{});
  }

  async_state(connect, system::error_code ec, lock_type read_lock, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);

    using next_tag = std::conditional_t<detail::has_ssl_v<next_layer_type>, do_handshake_tag, do_write_tag>;
    beast::get_lowest_layer(this_->next_layer_).async_connect(this_->endpoint_,
                         async_next(next, std::move(read_lock), std::move(write_lock)));
  }

  async_state(do_handshake, system::error_code ec, lock_type read_lock, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);
    detail::get_ssl_layer(this_->next_layer_)
        ->async_handshake(asio::ssl::stream_base::client,
                          async_next(disconnect, std::move(read_lock), std::move(write_lock)));
  }

  async_state(do_write, system::error_code ec, lock_type read_lock, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);

    request.set(beast::http::field::host, this_->host_);
    request.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    beast::http::async_write(this_->next_layer_, request, async_next(cpl_write, std::move(write_lock)));
  }

  async_state(cpl_write, system::error_code ec, std::size_t, lock_type write_lock)
  {
    if (ec == asio::error::broken_pipe || ec == asio::error::connection_reset)
      asem::async_lock(this_->read_mtx_, async_next(connect, std::move(write_lock)));
    else if (ec)
      async_complete(ec);
    else
      asem::async_lock(this_->read_mtx_, async_next(read_ready, std::move(write_lock)));

  }

  async_state(read_ready, system::error_code ec, lock_type read_lock, lock_type write_lock)
  {
    if (ec)
      async_complete(ec);

    beast::http::async_read(this_->next_layer_, this_->buffer_, response,
                            async_next(read_done, std::move(read_lock)));
  }
  async_state(read_done, system::error_code ec, std::size_t, lock_type read_lock)
  {
    now = std::chrono::system_clock::now();
    const auto conn_itr = response.find(beast::http::field::connection);
    if (ec || conn_itr == response.end())
    {
      this_->keep_alive_set_.timeout = std::chrono::system_clock::time_point::min();
      this_->keep_alive_set_.max = 0ull;
      return asem::async_lock(this_->write_mtx_, async_next(close_after, std::move(read_lock)));
    }

    if (urls::grammar::ci_is_equal(conn_itr->value(), "close"))
      return asem::async_lock(this_->write_mtx_, async_next(close_after, std::move(read_lock)));


    if (!urls::grammar::ci_is_equal(conn_itr->value(), "keep-alive"))
      async_complete(asio::error::invalid_argument);

    const auto kl_itr = response.find(beast::http::field::keep_alive);
    if (kl_itr == response.end())
      this_->keep_alive_set_ = keep_alive{}; // set to max
    else
    {

      auto rr = parse_keep_alive_field(kl_itr->value(), now);
      if (rr.has_error())
        ec = rr.error();
      else
        this_->keep_alive_set_ = *rr;

      if (this_->keep_alive_set_.timeout < now)
        return asem::async_lock(this_->write_mtx_, async_next(close_after, std::move(read_lock)));
    }
    async_complete(ec);
  }

  async_state(close_after, system::error_code ec,  lock_type write_lock, lock_type read_lock)
  {
    if (ec)
      async_complete(ec);
    do_disconnect<finish_tag>(std::move(self), ec, std::move(read_lock), std::move(write_lock),
                              detail::has_ssl<next_layer_type>{});
  }

  async_state(finish, system::error_code ec, lock_type , lock_type)
  {
    async_complete(ec);
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





}
}

#include <boost/asio/unyield.hpp>
#include <boost/requests/detail/undef.hpp>

#endif // BOOST_REQUESTS_IMPL_CONNECTION_HPP
