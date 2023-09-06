//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {


struct connection_pool::async_lookup_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  const urls::url_view sv;
  optional<asio::ip::tcp::resolver> resolver;

  urls::string_view scheme = "https";
  urls::string_view service;

  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};

  async_lookup_op(connection_pool * this_, urls::url_view sv, executor_type exec)
      : this_(this_), sv(sv), resolver(exec) {}

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code, typename asio::ip::tcp::resolver::results_type);


  BOOST_REQUESTS_DECL void resume(requests::detail::faux_token_t<step_signature_type> self,
                                  system::error_code & ec, typename asio::ip::tcp::resolver::results_type eps = {});
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
connection_pool::async_lookup(urls::url_view av, CompletionToken && completion_token)
{
  return detail::faux_run<async_lookup_op>(
      std::forward<CompletionToken>(completion_token),
      this, av, get_executor());
}


struct connection_pool::async_get_connection_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  async_get_connection_op(connection_pool * this_) : this_(this_) {}

  using conn_t = boost::unordered_multimap<endpoint_type,
                                           std::shared_ptr<detail::connection_impl>,
                                           detail::endpoint_hash>;
  typename conn_t::iterator itr;


  std::shared_ptr<detail::connection_impl> nconn = nullptr;
  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};
  endpoint_type ep;

  using completion_signature_type = void(system::error_code, connection);
  using step_signature_type       = void(system::error_code);

  BOOST_REQUESTS_DECL auto resume(requests::detail::faux_token_t<step_signature_type> self,
                                  system::error_code & ec) -> connection;
};

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
connection_pool::async_get_connection(CompletionToken && completion_token)
{
  // async_get_connection_op
  return detail::faux_run<async_get_connection_op>(
      std::forward<CompletionToken>(completion_token),
      this
      );
}

struct connection_pool::async_ropen_op : asio::coroutine
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

  connection conn;

  async_ropen_op(connection_pool * this_,
                 beast::http::verb method,
                 urls::pct_string_view path,
                 http::fields & headers,
                 source & src,
                 request_options opts,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path), headers(headers), src(src), opt(std::move(opts)), jar(jar)
  {
  }


  async_ropen_op(connection_pool * this_,
                 beast::http::verb method,
                 urls::url_view path,
                 http::fields & headers,
                 source & src,
                 request_options opt,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path.encoded_resource()),
        headers(headers), src(src), opt(std::move(opt)), jar(jar)
  {
  }

  using completion_signature_type = void(boost::system::error_code, stream);
  using step_signature_type       = void(boost::system::error_code, variant2::variant<variant2::monostate, connection, stream>);

  BOOST_REQUESTS_DECL
  auto resume(requests::detail::faux_token_t<step_signature_type> self,
              boost::system::error_code & ec,
              variant2::variant<variant2::monostate, connection, stream> res = variant2::monostate()) -> stream;

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
  return detail::faux_run<async_ropen_op>(
      std::forward<CompletionToken>(completion_token),
      this, method, path, std::ref(headers), std::ref(src), std::move(opt), jar);
}

template<typename RequestBody,
          typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, stream))
connection_pool::async_ropen(beast::http::verb method,
                             urls::url_view path,
                             RequestBody && body, request_parameters req,
                             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, stream)>(
      [this](auto handler,
             beast::http::verb method,
             urls::url_view path,
             RequestBody && body, request_parameters req)
      {
        auto source_ptr = requests::make_source(std::forward<RequestBody>(body));
        auto & source = *source_ptr;
        auto alloc = asio::get_associated_allocator(handler, asio::recycling_allocator<void>());
        auto header_ptr = allocate_unique<http::fields>(alloc, std::move(req.fields));
        auto & headers = *header_ptr;
        async_ropen(method, path, headers, source, std::move(req.opts), req.jar,
                    asio::consign(std::move(handler), std::move(source_ptr), std::move(header_ptr)));
      },
      completion_token, method, path, std::forward<RequestBody>(body), std::move(req)
  );
}

}
}

#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
