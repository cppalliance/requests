//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP

#include <boost/requests/connection_pool.hpp>
#include <boost/asio/yield.hpp>

namespace boost {
namespace requests {


struct connection_pool::async_lookup_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  const urls::url_view sv;
  optional<asio::ip::tcp::resolver> resolver;

  urls::string_view scheme = this_->use_ssl_ ? "https" : "http";
  urls::string_view service;

  using mutex_type = asem::mt::mutex;
  using lock_type = asem::lock_guard<mutex_type>;

  lock_type lock;

  async_lookup_op(connection_pool * this_, urls::url_view sv, executor_type exec)
      : this_(this_), sv(sv), resolver(exec) {}

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code, typename asio::ip::tcp::resolver::results_type);


  BOOST_REQUESTS_DECL void resume(requests::detail::co_token_t<step_signature_type> self,
                                  system::error_code & ec, typename asio::ip::tcp::resolver::results_type eps = {});
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
connection_pool::async_lookup(urls::url_view av, CompletionToken && completion_token)
{
  return detail::co_run<async_lookup_op>(
      std::forward<CompletionToken>(completion_token),
      this, av, get_executor());
}


struct connection_pool::async_get_connection_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  async_get_connection_op(connection_pool * this_) : this_(this_) {}

  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  using conn_t = boost::unordered_multimap<endpoint_type,
                                           std::shared_ptr<connection>,
                                           detail::endpoint_hash>;
  typename conn_t::iterator itr;


  std::shared_ptr<connection> nconn = nullptr;
  lock_type lock;
  endpoint_type ep;

  using completion_signature_type = void(system::error_code, std::shared_ptr<connection>);
  using step_signature_type       = void(system::error_code);

  BOOST_REQUESTS_DECL auto resume(requests::detail::co_token_t<step_signature_type> self,
                                  system::error_code & ec) -> std::shared_ptr<connection>;
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::shared_ptr<connection>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::shared_ptr<basic_connection<Stream>>))
connection_pool::async_get_connection(CompletionToken && completion_token)
{
  // async_get_connection_op
  return detail::co_run<async_get_connection_op>(
      std::forward<CompletionToken>(completion_token),
      this
      );
}

template<typename RequestBody>
struct connection_pool::async_ropen_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  beast::http::verb method;
  urls::url_view path;
  RequestBody && body;
  request_settings req;

  template<typename Self>
  void operator()(Self && self, error_code ec = {}, std::shared_ptr<connection> conn = nullptr)
  {
    reenter(this)
    {
      yield this_->async_get_connection(std::move(self));
      if (!ec && conn == nullptr)
        ec =  asio::error::not_found;
      if (ec)
        return self.complete(ec, stream{this_->get_executor(), nullptr});

      yield conn->async_ropen(method, path, std::forward<RequestBody>(body), std::move(req), std::move(self));
    }
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, stream res)
  {
    self.complete(ec, std::move(res));
  }
};


template<typename RequestBody,
         typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_pool::async_ropen(beast::http::verb method,
                             urls::url_view path,
                             RequestBody && body,
                             request_settings req,
                             CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, stream)>(
      async_ropen_op<RequestBody>{{}, this, method, path, std::forward<RequestBody>(body), std::move(req)},
      completion_token,
      mutex_
  );
}

struct connection_pool::async_ropen_op_src : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  beast::http::verb method;
  urls::pct_string_view path;
  http::fields & headers;
  source & src;
  request_options opt;
  cookie_jar * jar;

  template<typename Self>
  void operator()(Self && self, error_code ec = {}, std::shared_ptr<connection> conn = nullptr)
  {
    reenter(this)
    {
      yield this_->async_get_connection(std::move(self));
      if (!ec && conn == nullptr)
        ec =  asio::error::not_found;
      if (ec)
        return self.complete(ec, stream{this_->get_executor(), nullptr});

      yield conn->async_ropen(method, path, headers, src, std::move(opt), jar, std::move(self));
    }
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, stream res)
  {
    self.complete(ec, std::move(res));
  }
};


template<typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code,
                                   stream))
connection_pool::async_ropen(beast::http::verb method,
                             urls::pct_string_view path,
                             http::fields & headers,
                             source & src,
                             request_options opt,
                             cookie_jar * jar,
                             CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, stream)>(
      async_ropen_op_src{{}, this, method, path, headers, src, std::move(opt), jar},
      completion_token, mutex_);
}



}
}

#include <boost/asio/unyield.hpp>

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/connection_pool.ipp>
#endif


#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
