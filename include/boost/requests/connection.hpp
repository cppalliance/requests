// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_CONNECTION_HPP
#define BOOST_REQUESTS_CONNECTION_HPP

#include <boost/asem/mt.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/requests/detail/defaulted.hpp>
#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/requests/detail/ssl.hpp>
#include <boost/requests/fields/keep_alive.hpp>
#include <boost/requests/redirect.hpp>
#include <boost/requests/request_options.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/requests/response.hpp>
#include <boost/requests/source.hpp>
#include <boost/url/url_view.hpp>

namespace boost {
namespace requests {
struct stream;

struct connection
{
    /// The type of the next layer.
    typedef asio::ssl::stream<asio::generic::stream_protocol::socket> next_layer_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::executor_type executor_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

    /// This type with a defaulted completion token.
    template<typename Token>
    struct defaulted;

    /// Rebinds the socket type to another executor.
    template<typename Executor1, typename = void>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        using other = connection;
    };

    /// Get the executor
    executor_type get_executor() noexcept
    {
        return next_layer_.get_executor();
    }
    /// Get the underlying stream
    const next_layer_type &next_layer() const noexcept
    {
        return next_layer_;
    }

    /// Get the underlying stream
    next_layer_type &next_layer() noexcept
    {
        return next_layer_;
    }

    /// The protocol-type of the lowest layer.
    using protocol_type = asio::generic::stream_protocol;

    /// The endpoint of the lowest lowest layer.
    using endpoint_type = typename protocol_type::endpoint;


    connection(connection && lhs)
    : next_layer_(std::move(lhs.next_layer_))
    , use_ssl_(lhs.use_ssl_)
    , read_mtx_(std::move(lhs.read_mtx_))
    , write_mtx_(std::move(lhs.write_mtx_))
    , host_(std::move(lhs.host_))
    , buffer_(std::move(lhs.buffer_))
    , ongoing_requests_(std::move(lhs.ongoing_requests_.load()))
    , keep_alive_set_(std::move(lhs.keep_alive_set_))
    , endpoint_(std::move(lhs.endpoint_))
    {}

    connection & operator=(connection && lhs)
    {
       next_layer_ = std::move(lhs.next_layer_);
       use_ssl_ = lhs.use_ssl_;
       read_mtx_ = std::move(lhs.read_mtx_);
       write_mtx_ = std::move(lhs.write_mtx_);
       host_ = std::move(lhs.host_);
       buffer_ = std::move(lhs.buffer_);
       ongoing_requests_ = std::move(lhs.ongoing_requests_.load());
       keep_alive_set_ = std::move(lhs.keep_alive_set_);
       endpoint_ = std::move(lhs.endpoint_);
       return *this;
    }

    /// Construct a stream.
    template<typename ExecutorOrContext>
    explicit connection(ExecutorOrContext && exec_or_ctx, asio::ssl::context & ctx)
        : next_layer_(std::forward<ExecutorOrContext>(exec_or_ctx), ctx), use_ssl_{true} {}

    template<typename ExecutionContext>
    explicit connection(ExecutionContext &context,
                        typename asio::constraint<
                            asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                        >::type = 0)
        : next_layer_(
            context,
            asio::use_service<detail::ssl_context_service>(context).get()),
          use_ssl_{false} {}

    explicit connection(asio::any_io_executor exec)
      : next_layer_(
      exec,
      asio::use_service<detail::ssl_context_service>(
          asio::query(exec, asio::execution::context)
          ).get()), use_ssl_{false} {}

    void connect(endpoint_type ep)
    {
      boost::system::error_code ec;
      connect(ep, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void connect(endpoint_type ep,
                                     system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_connect(endpoint_type ep,
                  CompletionToken && completion_token);

    void close()
    {
      boost::system::error_code ec;
      close(ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void close(system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_close(CompletionToken && completion_token);

    bool is_open() const
    {
      return beast::get_lowest_layer(next_layer_).is_open();
    }

    // Endpoint
    endpoint_type endpoint() const {return endpoint_;}

    // Timeout of the connection-alive
    std::chrono::system_clock::time_point timeout() const
    {
      return keep_alive_set_.timeout;
    }

    std::size_t working_requests() const { return ongoing_requests_; }

    // Reserve memory for the internal buffer.
    void reserve(std::size_t size)
    {
      buffer_.reserve(size);
    }

    void set_host(core::string_view sv)
    {
      boost::system::error_code ec;
      set_host(sv, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void set_host(core::string_view sv, system::error_code & ec);
    core::string_view host() const {return host_;}
    constexpr static redirect_mode supported_redirect_mode() {return redirect_mode::endpoint;}

    using request_type = request_settings;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body,
               request_settings req,
               system::error_code & ec) -> stream;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body,
               request_settings req) -> stream;

    BOOST_REQUESTS_DECL
    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar,
               system::error_code & ec) -> stream;

    BOOST_REQUESTS_DECL
    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar) -> stream;

    template<typename RequestBody,
             typename CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                urls::url_view path,
                RequestBody && body,
                request_settings req,
                CompletionToken && completion_token);

    template<typename CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            stream))
    async_ropen(beast::http::verb method,
                urls::pct_string_view path,
                http::fields & headers,
                source & src,
                request_options opt,
                cookie_jar * jar,
                CompletionToken && completion_token);
    bool uses_ssl() const {return use_ssl_;}
  private:

    next_layer_type next_layer_;
    bool use_ssl_{false};
    asem::mt::mutex read_mtx_{next_layer_.get_executor()},
                    write_mtx_{next_layer_.get_executor()};

    std::string host_;
    beast::flat_buffer buffer_;
    std::atomic<std::size_t> ongoing_requests_{0u};
    keep_alive keep_alive_set_;
    endpoint_type endpoint_;

    struct async_close_op;
    struct async_connect_op;

    struct async_ropen_op;

    template<typename RequestSource>
    struct async_ropen_op_body;

    template<typename RequestSource>
    struct async_ropen_op_body_base;

    BOOST_REQUESTS_DECL std::size_t do_read_some_(beast::http::basic_parser<false> & parser);
    BOOST_REQUESTS_DECL std::size_t do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec) ;
    BOOST_REQUESTS_DECL void do_async_read_some_(beast::http::basic_parser<false> & parser, detail::faux_token_t<void(system::error_code, std::size_t)>) ;
    BOOST_REQUESTS_DECL void do_async_close_(detail::faux_token_t<void(system::error_code)>);
    BOOST_REQUESTS_DECL void do_close_(system::error_code & ec);

    friend struct stream;
};


template <typename Executor1>
struct connection::rebind_executor<Executor1, void_t<typename Executor1::default_completion_token_type>>
{
  using other = defaulted<typename Executor1::default_completion_token_type>;
};

namespace detail
{
template<typename Stream, typename Token>
struct defaulted_helper
{
  using type = typename Stream::template defaulted<Token>;
};

}


template<typename Token>
struct connection::defaulted : connection
{
  using default_token = Token;
  using connection::connection;
  using stream = typename detail::defaulted_helper<stream, Token>::type;
  defaulted(connection && lhs) :  connection(std::move(lhs)) {}
  using connection::async_connect;
  using connection::async_close;

  auto async_connect(endpoint_type ep)
  {
    return connection::async_connect(std::move(ep), default_token());
  }
  auto async_close()
  {
    return connection::async_close(default_token());
  }

  template<typename RequestBody,
            typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              request_settings req,
              CompletionToken && completion_token)
  {
    return connection::async_ropen(method, path, std::forward<RequestBody>(body), std::move(req),
                                   detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::pct_string_view path,
              http::fields & headers,
              source & src,
              request_options opt,
              cookie_jar * jar,
              CompletionToken && completion_token )
  {
    return connection::async_ropen(method, path, headers, src, std::move(opt), jar,
                                   detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename RequestBody>
  auto async_ropen(beast::http::verb method,
                   urls::url_view path,
                   RequestBody && body,
                   request_settings req)
  {
    return this->async_ropen(method, path, std::forward<RequestBody>(body), std::move(req), default_token());
  }

  template<typename RequestBody>
  auto async_ropen(http::request<RequestBody> & req,
                   request_options opt,
                   cookie_jar * jar = nullptr)
  {
    return this->async_ropen(req, std::move(opt), jar, default_token());
  }

};

}
}


#include <boost/requests/impl/connection.hpp>

#endif //BOOST_REQUESTS_CONNECTION_HPP
