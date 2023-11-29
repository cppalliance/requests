//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_WEBSOCKET_HPP
#define BOOST_REQUESTS_WEBSOCKET_HPP

#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace boost {
namespace requests {
namespace detail {

struct connection_impl
   ;
template<typename Executor = asio::any_io_executor>
struct optional_ssl_stream
{
  typedef asio::ssl::stream<asio::basic_stream_socket<asio::generic::stream_protocol, Executor>> next_layer_type;
  typedef typename next_layer_type::executor_type executor_type;
  typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

  executor_type get_executor() noexcept
  {
    return next_layer_.get_executor();
  }
  const next_layer_type &next_layer() const noexcept
  {
    return next_layer_;
  }

  next_layer_type &next_layer() noexcept
  {
    return next_layer_;
  }

  using protocol_type = asio::generic::stream_protocol;
  using endpoint_type = typename protocol_type::endpoint;

  template<typename MutableBufferSequence>
  std::size_t read_some(const MutableBufferSequence & buffers, system::error_code & ec)
  {
    if (use_ssl_)
      return next_layer().read_some(buffers, ec);
    else
      return next_layer().next_layer().read_some(buffers, ec);
  }

  template<typename ConstBufferSequence>
  std::size_t write_some(const ConstBufferSequence & buffers, system::error_code & ec)
  {
    if (uses_ssl())
      return next_layer().write_some(buffers, ec);
    else
      return next_layer().next_layer().write_some(buffers, ec);
  }

  template<typename MutableBufferSequence>
  std::size_t read_some(const MutableBufferSequence & buffers)
  {
    if (use_ssl_)
      return next_layer().read_some(buffers);
    else
      return next_layer().next_layer().read_some(buffers);
  }

  template<typename ConstBufferSequence>
  std::size_t write_some(const ConstBufferSequence & buffers)
  {
    if (uses_ssl())
      return next_layer().write_some(buffers);
    else
      return next_layer().next_layer().write_some(buffers);
  }

  template<
      typename MutableBufferSequence,
      typename ReadToken>
  auto async_read_some(const MutableBufferSequence & buffers, ReadToken && token)
  {
    if (uses_ssl())
      return next_layer().async_read_some(buffers, std::forward<ReadToken>(token));
    else
      return next_layer().next_layer().async_read_some(buffers, std::forward<ReadToken>(token));
  }

  template<
      typename ConstBufferSequence,
      typename WriteToken>
  auto async_write_some(const ConstBufferSequence & buffers, WriteToken && token)
  {
    if (uses_ssl())
      return next_layer().async_write_some(buffers, std::forward<WriteToken>(token));
    else
      return next_layer().next_layer().async_write_some(buffers, std::forward<WriteToken>(token));
  }

  /// Rebinds the socket type to another executor.
  template<typename Executor1>
  struct rebind_executor
  {
    /// The socket type when rebound to the specified executor.
    using other = optional_ssl_stream<Executor1>;
  };

  bool uses_ssl() const {return use_ssl_;}

  optional_ssl_stream(optional_ssl_stream && ) =default;

  template<typename Executor1>
  optional_ssl_stream(optional_ssl_stream<Executor1> && rhs) : next_layer_(std::move(rhs)), use_ssl_(rhs.use_ssl_) {}

private:
  next_layer_type next_layer_;
  bool use_ssl_{true};
  optional_ssl_stream(Executor exec) : next_layer_(exec, detail::ssl_context_service(exec.context()).get()) {}
  optional_ssl_stream(next_layer_type next_layer, bool use_ssl)
      : next_layer_(std::move(next_layer)), use_ssl_(use_ssl)
  {
  }

  friend struct connection_impl;
};

template<class Executor>
void
teardown(
    beast::role_type role,
    requests::detail::optional_ssl_stream<Executor>& socket,
    error_code& ec)
{
  if (socket.uses_ssl())
    boost::beast::teardown(role, socket.next_layer(), ec);
  else
    boost::beast::websocket::teardown(role, socket.next_layer().next_layer(), ec);
}

template<
    class Executor,
    class TeardownHandler>
void
async_teardown(
    beast::role_type role,
    requests::detail::optional_ssl_stream<Executor>& socket,
    TeardownHandler&& handler)
{

  if (socket.uses_ssl())
    boost::beast::async_teardown(role, socket.next_layer(), std::forward<TeardownHandler>(handler));
  else
    boost::beast::websocket::async_teardown(role, socket.next_layer().next_layer(), std::forward<TeardownHandler>(handler));

}

}

template<typename Executor = asio::any_io_executor>
using basic_websocket = boost::beast::websocket::stream<detail::optional_ssl_stream<Executor>>;

using websocket = basic_websocket<>;

}

namespace beast
{
namespace websocket
{





}
}

}

#endif // BOOST_REQUESTS_WEBSOCKET_HPP
