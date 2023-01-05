// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_STREAM_IPP
#define BOOST_REQUESTS_STREAM_IPP

#include <boost/requests/stream.hpp>
#include <boost/asio/yield.hpp>

namespace boost
{
namespace requests
{

void stream::dump(system::error_code & ec)
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

  bool should_close = interpret_keep_alive_response(impl_->keep_alive_set_, parser_->get(), ec);
  if (should_close)
  {
    boost::system::error_code ec_;
    impl_->close(ec_);
  }
}


stream::~stream()
{
  if (parser_ && parser_->is_header_done() && !parser_->is_done()
      && parser_->get().body().more && impl_ && impl_->is_open())
    dump();
}


std::size_t stream::async_read_some_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                               system::error_code & ec, std::size_t res)
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
      if (interpret_keep_alive_response(this_->impl_->keep_alive_set_, this_->parser_->get(), ec))
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


void stream::async_dump_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                   system::error_code ec, std::size_t n)
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

    if (interpret_keep_alive_response(this_->impl_->keep_alive_set_, this_->parser_->get(), ec))
    {
      ec_ = ec ;
      yield this_->impl_->do_async_close_(std::move(self));
      ec = ec_;
    }
  }
}


}
}

#include <boost/asio/unyield.hpp>

#endif //BOOST_REQUESTS_STREAM_IPP
