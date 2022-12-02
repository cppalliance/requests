//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_STREAM_HPP
#define BOOST_REQUESTS_IMPL_STREAM_HPP

#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/beast/core/buffer_ref.hpp>
#include <boost/requests/keep_alive.hpp>
#include <boost/requests/stream.hpp>

#include <boost/asio/yield.hpp>

namespace boost
{
namespace requests
{

template<typename Executor>
template<typename MutableBuffer>
std::size_t basic_stream<Executor>::read_some(const MutableBuffer & buffer, system::error_code & ec)
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

  auto res = impl_->do_read_some_(*parser_, ec);

  if (!parser_->is_done())
  {
    parser_->get().body().more = true;
    if (ec == beast::http::error::need_buffer)
      ec = {};
  }
  else
  {
    parser_->get().body().more = false;
    bool should_close = interpret_keep_alive_response(impl_->get_keep_alive_set_(), parser_->get(), ec);
    if (should_close)
    {
      boost::system::error_code ec_;
      impl_->do_close_(ec_);
      return res;
    }
  }

  return res;
};


template<typename Executor>
template<typename DynamicBuffer>
std::size_t basic_stream<Executor>::read(DynamicBuffer & buffer, system::error_code & ec)
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
    auto n = read_some(buffer.prepare(parser_->content_length_remaining().value_or(BOOST_REQUESTS_CHUNK_SIZE)), ec);
    buffer.commit(n);
    res += n;
  }

  if (!parser_->is_done())
    ec = beast::http::error::need_buffer;
  else
  {
    parser_->get().body().more = false;
    bool should_close = interpret_keep_alive_response(impl_->get_keep_alive_set_(), parser_->get(), ec);
    if (should_close)
    {
      boost::system::error_code ec_;
      impl_->do_close_(ec_);
      return res;
    }
  }
  return res;
}


template<typename Executor>
void basic_stream<Executor>::dump(system::error_code & ec)
{
  if (!parser_ || !parser_->get().body().more)
    return;

  char data[65535];
  while (parser_->get().body().more)
  {
    parser_->get().body().data = data;
    parser_->get().body().size = sizeof(data);
    impl_->do_read_some_(*parser_, ec);
    
    if (!parser_->is_done())
    {
      parser_->get().body().more = true;
      if (ec == beast::http::error::need_buffer)
        ec = {};
    }
    else
      parser_->get().body().more = false;
  }

  bool should_close = interpret_keep_alive_response(impl_->get_keep_alive_set_(), parser_->get(), ec);
  if (should_close)
  {
    boost::system::error_code ec_;
    impl_->do_close_(ec_);
  }
}



template<typename Executor>
template<typename DynamicBuffer>
struct basic_stream<Executor>::async_read_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_stream * this_;
  DynamicBuffer & buffer;

  template<typename DynamicBuffer_>
  async_read_op(basic_stream * this_, DynamicBuffer_ && buffer) : this_(this_), buffer(buffer)
  {
  }

  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  lock_type lock;
  system::error_code ec_;
  std::size_t res = 0u;


  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  std::size_t resume(requests::detail::co_token_t<step_signature_type> self,
                     system::error_code & ec, std::size_t n = 0u)
  {
    reenter(this)
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
        yield this_->async_read_some(
            buffer.prepare(this_->parser_->content_length_remaining().value_or(BOOST_REQUESTS_CHUNK_SIZE)),
            std::move(self));
        buffer.commit(n);
        res += n;
      }

      if (!this_->parser_->is_done() && !ec)
        BOOST_REQUESTS_ASSIGN_EC(ec, beast::http::error::need_buffer)
      else
      {
        this_->parser_->get().body().more = false;
        if (interpret_keep_alive_response(this_->impl_->get_keep_alive_set_(), this_->parser_->get(), ec))
        {
          std::swap(ec, ec_);
          yield this_->impl_->do_async_close_(std::move(self));
          std::swap(ec, ec_);
        }
      }

      return res;
    }
    return 0u;
  }

};


template<typename Executor>
template<
    typename DynamicBuffer,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
basic_stream<Executor>::async_read(
    DynamicBuffer & buffers,
    CompletionToken && token)
{
  return detail::co_run<async_read_op<DynamicBuffer>>(std::forward<CompletionToken>(token), this, buffers);
}

template<typename Executor>
struct basic_stream<Executor>::async_read_some_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_stream * this_;
  asio::mutable_buffer buffer;

  template<typename MutableBufferSequence>
  async_read_some_op(basic_stream * this_, const MutableBufferSequence & buffer) : this_(this_)
  {
    auto itr = boost::asio::buffer_sequence_begin(buffer);
    const auto end = boost::asio::buffer_sequence_end(buffer);

    while (itr != end)
    {
      if (itr->size() != 0u)
      {
        this->buffer = *itr;
        break;
      }
    }
  }

  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  lock_type lock;
  system::error_code ec_;


  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  std::size_t resume(requests::detail::co_token_t<step_signature_type> self,
                     system::error_code & ec, std::size_t res = 0u)
  {

    reenter(this)
    {
      if (!this_->parser_)
        BOOST_REQUESTS_ASSIGN_EC(ec_, asio::error::not_connected)
      else if (!this_->parser_->get().body().more)
        BOOST_REQUESTS_ASSIGN_EC(ec_, asio::error::eof)

      if (ec_)
      {
        yield asio::post(this_->get_executor(), std::move(self));
        ec = ec_;
        return 0u;
      }

      if (buffer.size() == 0u)
      {
        yield asio::post(this_->get_executor(), asio::append(std::move(self), ec_));
        return 0u;
      }

      this_->parser_->get().body().data = buffer.data();
      this_->parser_->get().body().size = buffer.size();

      yield this_->impl_->do_async_read_some_(*this_->parser_, std::move(self));
      if (!this_->parser_->is_done())
      {
        this_->parser_->get().body().more = true;
        if (ec == beast::http::error::need_buffer)
          ec = {};
      }
      else
      {
        this_->parser_->get().body().more = false;
        if (interpret_keep_alive_response(this_->impl_->get_keep_alive_set_(), this_->parser_->get(), ec))
        {
          ec_ = ec ;
          yield this_->impl_->do_async_close_(std::move(self));
          ec = ec_;
        }
      }
      return res;
    }
    return 0u;
  }

};

template<typename Executor>
template<
    typename MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
basic_stream<Executor>::async_read_some(
    const MutableBufferSequence & buffers,
    CompletionToken && token)
{
  return detail::co_run<async_read_some_op>(std::forward<CompletionToken>(token), this, buffers);
}


template<typename Executor>
struct basic_stream<Executor>::async_dump_op : asio::coroutine
{
  using executor_type = Executor;
  executor_type get_executor() {return this_->get_executor(); }

  basic_stream * this_;
  using mutex_type = detail::basic_mutex<executor_type>;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  lock_type lock;
  system::error_code ec_;

  async_dump_op(basic_stream * this_) : this_(this_) {}

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code, std::size_t);

  void resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code ec = {}, std::size_t n = 0u)
  {
    reenter(this)
    {
      if (!this_->parser_ || !this_->parser_->is_done())
      {
        yield asio::post(this_->get_executor(), std::move(self));
        ec = ec_;
        return;
      }

      while (!this_->parser_->is_done())
      {
        this_->parser_->get().body().data = buffer;
        this_->parser_->get().body().size = BOOST_REQUESTS_CHUNK_SIZE;

        yield this_->impl_->do_async_read_some_(*this_->parser_, std::move(self));
      }

      if (interpret_keep_alive_response(this_->impl_->get_keep_alive_set_(), this_->parser_->get(), ec))
      {
        ec_ = ec ;
        yield this_->impl_->do_async_close_(std::move(self));
        ec = ec_;
      }
    }
  }
};

template<typename Executor>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
basic_stream<Executor>::async_dump(CompletionToken && token)
{
  return detail::co_run<async_dump_op>(std::forward<CompletionToken>(token), this);
}



template<typename Executor>
basic_stream<Executor>::~basic_stream()
{
  if (parser_ && parser_->is_header_done() && !parser_->is_done()
      && parser_->get().body().more && impl_ && impl_->is_open())
    dump();
}

}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_STREAM_HPP
