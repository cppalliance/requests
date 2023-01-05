//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_IPP
#define BOOST_REQUESTS_IMPL_CONNECTION_IPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/asio/yield.hpp>

namespace boost
{
namespace requests
{

void connection::set_host(core::string_view sv, system::error_code & ec)
{
  next_layer_.set_verify_callback(asio::ssl::host_name_verification(host_ = sv), ec);
}


auto connection::async_ropen_file_op::resume(requests::detail::co_token_t<step_signature_type> self,
            system::error_code & ec, std::size_t res) -> stream
{
  return async_ropen_op<http::file_body>::resume(std::move(self), ec, res);
}


auto connection::async_ropen_string_op::resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res) -> stream
{
  return async_ropen_op<http::string_body>::resume(std::move(self), ec, res);
}

auto connection::async_ropen_empty_op::resume(requests::detail::co_token_t<step_signature_type> self,
              system::error_code & ec, std::size_t res) -> stream
{
  return async_ropen_op<http::empty_body>::resume(std::move(self), ec, res);
}


void connection::connect(endpoint_type ep, system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;

  next_layer_.next_layer().connect(endpoint_ = ep, ec);

  if (use_ssl_ && !ec)
    next_layer_.handshake(asio::ssl::stream_base::client, ec);
}


void connection::close(system::error_code & ec)
{
  auto wlock = asem::lock(write_mtx_, ec);
  if (ec)
    return;

  auto rlock = asem::lock(read_mtx_, ec);
  if (ec)
    return;

  if (use_ssl_)
    next_layer_.shutdown(ec);

  if (ec)
    return ;

  next_layer_.next_layer().close(ec);
}


std::size_t connection::do_read_some_(beast::http::basic_parser<false> & parser)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser);
}

std::size_t connection::do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec)
{
  if (use_ssl_)
    return beast::http::read_some(next_layer_, buffer_, parser, ec);
  else
    return beast::http::read_some(next_layer_.next_layer(), buffer_, parser, ec);
}

void connection::do_async_read_some_(beast::http::basic_parser<false> & parser, detail::co_token_t<void(system::error_code, std::size_t)> tk)
{
  if (use_ssl_)
    beast::http::async_read_some(next_layer_, buffer_, parser, std::move(tk));
  else
    beast::http::async_read_some(next_layer_.next_layer(), buffer_, parser, std::move(tk));
}

void connection::do_async_close_(detail::co_token_t<void(system::error_code)> tk)
{
  async_close(std::move(tk));
}

auto connection::ropen(http::request<http::empty_body> & req,  request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::empty_body>(req, std::move(opt), jar, ec);
}

auto connection::ropen(http::request<http::file_body> & req,   request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::file_body>(req, std::move(opt), jar, ec);
}

auto connection::ropen(http::request<http::string_body> & req, request_options opt, cookie_jar * jar, system::error_code & ec) -> stream
{
  return ropen<http::string_body>(req, std::move(opt), jar, ec);
}

void connection::async_connect_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                          system::error_code & ec)
{
  reenter(this)
  {
    await_lock(this_->write_mtx_, write_lock);
    await_lock(this_->read_mtx_,  read_lock);
    yield this_->next_layer_.next_layer().async_connect(this_->endpoint_ = ep, std::move(self));
    if (!ec && this_->use_ssl_)
    {
      yield this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
    }
  }
}

void connection::async_close_op::resume(requests::detail::co_token_t<step_signature_type> self,
                                          system::error_code & ec)
{
  reenter(this)
  {
    await_lock(this_->write_mtx_, write_lock);
    await_lock(this_->read_mtx_,  read_lock);
    if (!ec && this_->use_ssl_)
    {
      yield this_->next_layer_.async_shutdown(std::move(self));
    }
    if (!ec)
      this_->next_layer_.next_layer().close(ec);
  }
}

}
}

#include <boost/requests/detail/undefine.hpp>
#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_REQUESTS_CONNECTION_IPP
