//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_IPP
#define BOOST_REQUESTS_IMPL_SESSION_IPP


#if defined(BOOST_REQUESTS_SOURCE)

#include <boost/requests/session.hpp>
#include <boost/requests/detail/define.hpp>
#include <boost/asio/yield.hpp>

namespace boost {
namespace requests {

auto session::make_request_(http::fields fields) -> requests::request_settings
{
  return requests::request_settings{
      std::move(fields),
      options_,
      &jar_
  };
}

urls::url session::normalize_(urls::url_view in)
{
  urls::url res;
  if (!in.has_scheme() || in.scheme() == "https")
  {
    res.set_scheme("https");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else if (in.scheme() == "http")
  {
    res.set_scheme("http");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else
    return in; // just do some invalid shit
}


auto
session::get_pool(urls::url_view url, error_code & ec) -> std::shared_ptr<connection_pool>
{
  // can be optimized to ellide the string allocation, blabla (pmr?)
  char buf[1024];
  container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};
  using alloc = container::pmr::polymorphic_allocator<char>;
  using str = std::basic_string<char, std::char_traits<char>, alloc>;
  auto host_key = normalize_(url);

  const auto is_https = (url.scheme_id() == urls::scheme::https)
                        || (url.scheme_id() == urls::scheme::wss);
  auto lock = asem::lock(mutex_, ec);
  if (ec)
    return std::shared_ptr<connection_pool>();

  auto itr = pools_.find(host_key);
  if (itr != pools_.end())
    return itr->second;
  else
  {
    auto p = std::make_shared<connection_pool>(get_executor(), sslctx_);
    p->lookup(host_key, ec);
    if (!ec)
    {
      pools_.emplace(host_key, p);
      return p;
    }
  }
  return nullptr;
}

std::shared_ptr<connection_pool> session::async_get_pool_op::resume(
    requests::detail::co_token_t<step_signature_type> self,
    error_code ec)
{
  reenter(this)
  {
    await_lock(this_->mutex_, lock);

    {
      auto itr = this_->pools_.find(url);
      if (itr != this_->pools_.end())
        return itr->second;
    }
    p = std::make_shared<connection_pool>(this_->get_executor(), this_->sslctx_);
    yield p->async_lookup(url, std::move(self));
    if (!ec)
    {
      this_->pools_.emplace(url, p);
      return p;
    }
  }
  return nullptr;
}

}
}

#include <boost/asio/unyield.hpp>
#include <boost/requests/detail/undefine.hpp>

#endif


#endif // BOOST_REQUESTS_IMPL_SESSION_IPP
