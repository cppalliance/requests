// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/stream.hpp>
#include <boost/requests/detail/connection_impl.hpp>
#include <boost/requests/detail/define.hpp>

#include <boost/asio/dispatch.hpp>

namespace boost
{
namespace requests
{

void stream::dump(system::error_code & ec)
{
  if (!parser_ || !parser_->get().body().more || parser_->is_done())
    return;

  char data[65535];
  while (!ec && parser_->get().body().more && !parser_->is_done())
  {
    parser_->get().body().data = data;
    parser_->get().body().size = sizeof(data);
    impl_->do_read_some_(*parser_, ec);

    if (!ec && !parser_->is_done())
    {
      parser_->get().body().more = true;
      if (ec == beast::http::error::need_buffer)
        ec = {};
    }
    else
      parser_->get().body().more = false;
  }

  if (ec || !parser_->get().keep_alive())
  {
    boost::system::error_code ec_;
    impl_->do_close_(ec_);
  }
  /*else
    impl_->do_return_();*/
}


stream::~stream()
{
  if (parser_ && parser_->is_header_done() && !parser_->is_done()
      && parser_->get().body().more && impl_ && impl_->is_open())
    dump();
}


struct stream::async_read_some_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  stream * this_;
  asio::mutable_buffer buffer;

  template<typename MutableBufferSequence>
  async_read_some_op(stream * this_, const MutableBufferSequence & buffer) : this_(this_)
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
  system::error_code ec_;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t res = 0u);
};


template<typename Self>
void stream::async_read_some_op::operator()(Self && self, system::error_code ec, std::size_t res)
{
  if (!ec)
    BOOST_ASIO_CORO_REENTER(this)
    {
      if (!this_->parser_)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_connected)
      else if (!this_->parser_->get().body().more)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::eof)
      else if (buffer.size() == 0u)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::no_buffer_space)

      if (ec)
        break;

      this_->parser_->get().body().data = buffer.data();
      this_->parser_->get().body().size = buffer.size();

      BOOST_ASIO_CORO_YIELD this_->impl_->do_async_read_some_(*this_->parser_, std::move(self));
      if (!this_->parser_->is_done())
      {
        this_->parser_->get().body().more = true;
        if (ec == beast::http::error::need_buffer)
          ec = {};
      }
      else
      {
        this_->parser_->get().body().more = false;
        if (!this_->parser_->get().keep_alive())
        {
          ec_ = ec ;
          BOOST_ASIO_CORO_YIELD this_->impl_->do_async_close_(std::move(self));
          ec = ec_;
        }
      }
    }

  if (is_complete())
    self.complete(ec, res);
}

void stream::async_read_some_impl(
    asio::any_completion_handler<void(error_code, std::size_t)> handler,
    asio::mutable_buffer buffer)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code, std::size_t)>,
       void(error_code, std::size_t)>(
      async_read_some_op{this, buffer},
      handler, get_executor());
}


struct stream::async_dump_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  stream * this_;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  system::error_code ec_;

  async_dump_op(stream * this_) : this_(this_) {}

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t n = 0u);
};

template<typename Self>
void stream::async_dump_op::operator()(Self && self, system::error_code ec, std::size_t n)
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    if (!this_->parser_ || !this_->parser_->is_done())
    {
      BOOST_ASIO_CORO_YIELD asio::dispatch(
            asio::get_associated_immediate_executor(self, this_->get_executor()),
            std::move(self));
      ec = ec_;
      return;
    }

    while (!ec && !this_->parser_->is_done())
    {
      this_->parser_->get().body().data = buffer;
      this_->parser_->get().body().size = BOOST_REQUESTS_CHUNK_SIZE;

      BOOST_ASIO_CORO_YIELD this_->impl_->do_async_read_some_(*this_->parser_, std::move(self));
    }

    if (ec || !this_->parser_->get().keep_alive())
    {
      ec_ = ec ;
      BOOST_ASIO_CORO_YIELD this_->impl_->do_async_close_(std::move(self));
      ec = ec_;
    }
  }
  if (is_complete())
    self.complete(ec);
}

bool stream::is_open() const
{
  return impl_ && impl_->is_open() && !done();
}

void stream::async_dump_impl(
    asio::any_completion_handler<void(error_code)> handler, stream * this_)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code)>,
      void(error_code)>(
      async_dump_op{this_},
      handler, this_->get_executor());
}


}
}

