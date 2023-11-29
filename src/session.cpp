//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <boost/requests/detail/define.hpp>
#include <boost/requests/session.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/beast/version.hpp>

namespace boost {
namespace requests {

auto session::make_request_(http::fields fields) -> requests::request_parameters {
  return requests::request_parameters{
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
    if (!in.has_scheme())
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
  auto host_key = normalize_(url);

  std::lock_guard<std::mutex> lock(mutex_);
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

struct session::async_get_pool_op : asio::coroutine
{
  constexpr static const char * op_name = "session::async_get_pool_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session *this_;
  urls::url_view url;

  async_get_pool_op(session *this_, urls::url_view url)
      : this_(this_), url(url)
  {}

  std::shared_ptr<connection_pool> p;

  template<typename Self>
  void operator()(Self && self, error_code ec = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {

      {
        std::lock_guard<std::mutex> lock{this_->mutex_};
        auto itr = this_->pools_.find(url);
        if (itr != this_->pools_.end())
          return self.complete(ec, std::move(itr->second));
      }
      p = std::make_shared<connection_pool>(this_->get_executor(), this_->sslctx_);
      BOOST_REQUESTS_YIELD p->async_lookup(url, std::move(self));
      if (!ec)
      {
        std::lock_guard<std::mutex> lock{this_->mutex_};
        auto itr = this_->pools_.find(url);
        if (itr == this_->pools_.end())
          this_->pools_.emplace(url, p);
        else
          p = itr->second;
      }
      return self.complete(ec, p);
    }
  }

};

void session::async_get_pool_impl(
    asio::any_completion_handler<void (boost::system::error_code, std::shared_ptr<connection_pool>)> handler,
    session * sess, urls::url_view url)
{
  asio::async_compose<
      asio::any_completion_handler<void (boost::system::error_code, std::shared_ptr<connection_pool>)>,
      void (boost::system::error_code, std::shared_ptr<connection_pool>)>(
        async_get_pool_op{sess, url},
        handler,
        sess->get_executor()
      );
}

}
}

#include <boost/requests/detail/undefine.hpp>

