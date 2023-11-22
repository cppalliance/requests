//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_STREAM_HPP
#define BOOST_REQUESTS_IMPL_STREAM_HPP

#include <boost/requests/stream.hpp>
#include <boost/requests/detail/connection_impl.hpp>

#include <boost/asio/compose.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>


namespace boost
{
namespace requests
{

template<typename MutableBuffer>
std::size_t stream::read_some(const MutableBuffer & buffer, system::error_code & ec)
{
  if (!parser_)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_connected);
    return 0u;
  }
  else if (!parser_->get().body().more)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::eof);
    return 0u;
  }

  auto itr = boost::asio::buffer_sequence_begin(buffer);
  const auto end = boost::asio::buffer_sequence_end(buffer);
  if (itr == end)
    return 0u;

  parser_->get().body().data = itr->data();
  parser_->get().body().size = itr->size();
  if (parser_->chunked())
  {
    impl_->handle_chunked_.chunked_body = {};
    impl_->handle_chunked_.buffer_space = itr->size();
  }

  auto res = impl_->do_read_some_(*parser_, ec);
  if (parser_->chunked())
    res = asio::buffer_copy(*itr, asio::buffer(impl_->handle_chunked_.chunked_body, impl_->handle_chunked_.chunked_body.size()));
  if (!parser_->is_done())
  {
    parser_->get().body().more = true;
    if (ec == beast::http::error::need_buffer)
      ec = {};
  }
  else
  {
    parser_->get().body().more = false;
    if (!parser_->get().keep_alive())
    {
      boost::system::error_code ec_;
      impl_->do_close_(ec_);
      return res;
    }
  }

  return res;
};


template<typename DynamicBuffer>
std::size_t stream::read(DynamicBuffer & buffer, system::error_code & ec)
{
  if (!parser_)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_connected);
    return 0u;
  }
  else if (!parser_->get().body().more)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::eof);
    return 0u;
  }

  std::size_t res = 0u;
  while (!ec && !parser_->is_done())
  {
    auto max_size = (std::min)(parser_->content_length_remaining().value_or(BOOST_REQUESTS_CHUNK_SIZE),
                               buffer.max_size() - buffer.size());
    if (buffer.max_size() - buffer.size() == 0u)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, beast::http::error::need_buffer);
      return res;
    }
    auto n = read_some(buffer.prepare(max_size), ec);
    buffer.commit(n);
    res += n;
  }

  if (!parser_->is_done())
    ec = beast::http::error::need_buffer;
  else
  {
    parser_->get().body().more = false;
    if (!parser_->get().keep_alive())
    {
      boost::system::error_code ec_;
      impl_->do_close_(ec_);
      return res;
    }
  }
  return res;
}

template<typename DynamicBuffer>
struct stream::async_read_op : asio::coroutine
{
  constexpr static const char * op_name = "stream::async_read_op";
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  stream * this_;
  DynamicBuffer & buffer;

  async_read_op(stream * this_, DynamicBuffer & buffer) : this_(this_), buffer(buffer)
  {
  }

  system::error_code ec_;
  std::size_t res = 0u;

  template<typename Self>
  void operator()(Self && self, error_code ec = {}, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      if (!this_->parser_)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_connected)
      else if (!this_->parser_->get().body().more)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::eof)

      if (ec)
        break;

      if (this_->parser_->is_done())
        break;

      while (!ec && !this_->parser_->is_done())
      {
        BOOST_REQUESTS_YIELD
        {
          auto max_size = (std::min)(this_->parser_->content_length_remaining().value_or(BOOST_REQUESTS_CHUNK_SIZE),
                                     buffer.max_size() - buffer.size());
          this_->async_read_some(buffer.prepare(max_size), std::move(self));
        }
        buffer.commit(n);
        res += n;
      }

      if (ec)
        break;

      if (!this_->parser_->is_done() && !ec)
        BOOST_REQUESTS_ASSIGN_EC(ec, beast::http::error::need_buffer)
      else
      {
        this_->parser_->get().body().more = false;
        if (this_->parser_->get().keep_alive())
        {
          std::swap(ec, ec_);
          BOOST_REQUESTS_YIELD this_->impl_->do_async_close_(std::move(self));
          std::swap(ec, ec_);
        }
      }
    }
    if (is_complete())
      self.complete(ec, res);
  }

};


template<
    typename DynamicBuffer,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
stream::async_read(
    DynamicBuffer & buffers,
    CompletionToken && token)
{
  return asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
          async_read_op<DynamicBuffer>{this, buffers},
          token, get_executor());
}

template<
    typename MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
stream::async_read_some(
    const MutableBufferSequence & buffers,
    CompletionToken && token)
{
  return asio::async_initiate<CompletionToken, void(error_code, std::size_t)>(
      [this](asio::any_completion_handler<void(error_code, std::size_t)> handler,
         const MutableBufferSequence & buffers)
      {
        auto itr = boost::asio::buffer_sequence_begin(buffers);
        const auto end = boost::asio::buffer_sequence_end(buffers);

        asio::mutable_buffer buffer;
        while (itr != end)
        {
          if (itr->size() != 0u)
          {
            buffer = *itr;
            break;
          }
        }
        async_read_some_impl(std::move(handler), buffer);
      }, token, buffers);
}



template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
stream::async_dump(CompletionToken && token)
{
  return asio::async_initiate<CompletionToken, void(error_code)>(&async_dump_impl, token, this);
}

}
}

#endif // BOOST_REQUESTS_IMPL_STREAM_HPP
