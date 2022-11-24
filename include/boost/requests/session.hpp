// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_BASIC_SESSION_HPP
#define BOOST_REQUESTS_BASIC_SESSION_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/requests/connection_pool.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/string.hpp>
#include <boost/container/pmr/synchronized_pool_resource.hpp>


namespace boost::requests
{

template<typename Executor = asio::any_io_executor>
struct basic_session
{
    /// The type of the executor associated with the object.
    typedef Executor executor_type;

    /// Rebinds the timer type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The timer type when rebound to the specified executor.
        typedef basic_session<Executor1> other;
    };

    /// Constructor.
    explicit basic_session(const executor_type &ex)
            : mutex_(ex)
    {
      sslctx_.set_verify_mode(asio::ssl::verify_peer);
      sslctx_.set_default_verify_paths();
    }

    /// Constructor.
    template<typename ExecutionContext>
    explicit basic_session(ExecutionContext &context,
                     typename asio::constraint<
                                   asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                           >::type = 0)
            : mutex_(context.get_executor())
    {
      sslctx_.set_verify_mode(asio::ssl::verify_peer);
      sslctx_.set_default_verify_paths();
    }

    /// Rebinding construcotr.
    template<typename Executor2>
    explicit basic_session(basic_session<Executor2> && sess)
        : mutex_(std::move(sess.mutex))
    {
      sslctx_.set_verify_mode(asio::ssl::verify_peer);
      sslctx_.set_default_verify_paths();
    }

    /// Get the executor associated with the object.
    executor_type get_executor() BOOST_ASIO_NOEXCEPT
    {
        return mutex_.get_executor();
    }

          struct request_options & options()       {return options_;}
    const struct request_options & options() const {return options_;}

    template<typename RequestBody>
    auto request(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 http::fields req,
                 system::error_code & ec) -> response;

    template<typename RequestBody>
    auto request(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 http::fields req)
        -> response
    {
      boost::system::error_code ec;
      auto res = request(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "request"));
      return res;
    }

    template<typename RequestBody>
    auto request(beast::http::verb method,
                 core::string_view path,
                 RequestBody && body,
                 http::fields req,
                 system::error_code & ec) -> response
    {
      auto url = urls::parse_uri(path);
      if (url.has_error())
      {
        ec = url.error();
        return response{req.get_allocator()};
      }
      else
        return request(method, url.value(), std::forward<RequestBody>(body), req, ec);
    }

    template<typename RequestBody>
    auto request(beast::http::verb method,
                 core::string_view path,
                 RequestBody && body,
                 http::fields req)
        -> response
    {
      boost::system::error_code ec;
      auto res = request(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "request"));
      return res;
    }


    template<typename RequestBody,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   response)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            response))
    async_request(beast::http::verb method,
                  urls::url_view path,
                  RequestBody && body,
                  http::fields req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename RequestBody,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   response)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            response))
    async_request(beast::http::verb method,
                  core::string_view path,
                  RequestBody && body,
                  http::fields req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    auto download(urls::url_view path,
                  http::fields req,
                  const filesystem::path & download_path,
                  system::error_code & ec) -> response;

    auto download(urls::url_view path,
                  http::fields req,
                  const filesystem::path & download_path) -> response
    {
      boost::system::error_code ec;
      auto res = download(path, std::move(req), download_path, ec);
      if (ec)
        throw_exception(system::system_error(ec, "download"));
      return res;
    }

    auto download(core::string_view path,
                  http::fields req,
                  const filesystem::path & download_path,
                  system::error_code & ec) -> response
    {
      auto url = urls::parse_uri(path);
      if (url.has_error())
      {
        ec = url.error();
        return response{req.get_allocator()};
      }
      else
        return download(url.value(), req, download_path, ec);
    }

    auto download(core::string_view path,
                  http::fields req,
                  const filesystem::path & download_path) -> response
    {
      boost::system::error_code ec;
      auto res = download(path, std::move(req), download_path, ec);
      if (ec)
        throw_exception(system::system_error(ec, "download"));
      return res;
    }


    template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   response)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            response))
    async_download(urls::url_view path,
                   http::fields req,
                   filesystem::path download_path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   response)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            response))
    async_download(core::string_view path,
                   http::fields req,
                   filesystem::path download_path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    using request_type = http::fields;
    using target_view =  core::string_view;

    // possibly make it a distinct return type.
    using pool_ptr = variant2::variant<std::shared_ptr<basic_http_connection_pool<Executor>>,
                                       std::shared_ptr<basic_https_connection_pool<Executor>>>;
    pool_ptr get_pool(urls::url_view url, error_code & ec);
    pool_ptr get_pool(urls::url_view url)
    {
      boost::system::error_code ec;
      auto res = get_pool(url, ec);
      if (ec)
        throw_exception(system::system_error(ec, "get_pool"));
      return res;
    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, pool_ptr)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
    async_get_pool(urls::url_view path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    void shutdown()
    {
      http_pools_.clear();
      https_pools_.clear();
    }
    struct stream;
    
    
    template<typename RequestBody>
    auto ropen(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 http::fields req,
                 system::error_code & ec) -> stream;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 http::fields req) -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "ropen"));
      return res;
    }

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
                 core::string_view path,
                 RequestBody && body,
                 http::fields req,
                 system::error_code & ec) -> stream
    {
      auto url = urls::parse_uri(path);
      if (url.has_error())
      {
        ec = url.error();
        return response{req.get_allocator()};
      }
      else
        return ropen(method, url.value(), std::forward<RequestBody>(body), req, ec);
    }

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
                 core::string_view path,
                 RequestBody && body,
                 http::fields req)
        -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "ropen"));
      return res;
    }


    template<typename RequestBody,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                  urls::url_view path,
                  RequestBody && body,
                  http::fields req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename RequestBody,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                  core::string_view path,
                  RequestBody && body,
                  http::fields req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename Request, typename Response,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
    async_single_request(Request & req, Response & res, urls::url_view url,
                         CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

  private:
    asio::ssl::context sslctx_{asio::ssl::context_base::tls_client};
    detail::basic_mutex<executor_type> mutex_;

    struct request_options options_{default_options()};

    using host_ = std::pair<
        std::basic_string<char, std::char_traits<char>, container::pmr::polymorphic_allocator<char>>, unsigned short>;

    boost::unordered_map<host_, std::shared_ptr<basic_http_connection_pool<Executor>>> http_pools_;
    boost::unordered_map<host_, std::shared_ptr<basic_https_connection_pool<Executor>>> https_pools_;

    // this isn't great
    boost::container::pmr::synchronized_pool_resource pmr_;
    cookie_jar jar_{boost::container::pmr::polymorphic_allocator<char>(&pmr_)};

    template<typename RequestBody>
    struct async_request_op;

    struct async_download_op;

    struct async_get_pool_op;

    template<typename Request, typename Response>
    struct async_single_request_op;

    auto make_request_(http::fields fields) -> requests::request_settings;

    template<typename>
    friend struct basic_session;
};


template<typename Executor>
struct basic_session<Executor>::stream
{
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  /// Get the executor
  executor_type get_executor() noexcept
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).get_executor();
    case 1: return variant2::get<1>(impl_).get_executor();
    }
  }

  /// Check if the underlying connection is open.
  bool is_open() const
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).is_open();
    case 1: return variant2::get<1>(impl_).is_open();
    }
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer)
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).read_some(buffer);
    case 1: return variant2::get<1>(impl_).read_some(buffer);
    }
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer, system::error_code & ec)
  {
    {
      switch (impl_.index())
      {
      default:
      case 0: return variant2::get<0>(impl_).read_some(buffer, ec);
      case 1: return variant2::get<1>(impl_).read_some(buffer, ec);
      }
    }
  }

  /// Read some data from the request body.
  template<
      typename MutableBufferSequence,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken
          BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
  async_read_some(
      const MutableBufferSequence & buffers,
      CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).async_read_some(buffers, std::forward<CompletionToken>(token));
    case 1: return variant2::get<1>(impl_).async_read_some(buffers, std::forward<CompletionToken>(token));
    }
  }


  /// dump the rest of the data.
  void dump()
  {
    switch (impl_.index())
    {
    case 0: return variant2::get<0>(impl_).dump();
    case 1: return variant2::get<1>(impl_).dump();
    }
  }

  void dump(system::error_code & ec)
  {
    switch (impl_.index())
    {
    case 0: return variant2::get<0>(impl_).dump(ec);
    case 1: return variant2::get<1>(impl_).dump(ec);
    }
  }

  /// Read some data from the request body.
  template<
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
          CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
  async_dump(CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).async_dump(std::forward<CompletionToken>(token));
    case 1: return variant2::get<1>(impl_).async_dump(std::forward<CompletionToken>(token));
    }
  }


  stream           (stream &&) = default;
  stream& operator=(stream &&) = default;

  stream           (const stream &) = delete;
  stream& operator=(const stream &) = delete;
  ~stream() = default;

  const http::response_header &headers() const
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).headers();
    case 1: return variant2::get<1>(impl_).headers();
    }
  }

  bool done() const
  {
    switch (impl_.index())
    {
    default:
    case 0: return variant2::get<0>(impl_).done();
    case 1: return variant2::get<1>(impl_).done();
    }
  }

  explicit stream(std::nullptr_t) : impl_(typename basic_http_connection<Executor>::stream(nullptr)) {}

private:
  explicit stream(typename basic_http_connection<Executor>::stream p)  : impl_(std::move(p)) {}
  explicit stream(typename basic_https_connection<Executor>::stream p) : impl_(std::move(p)) {}
  variant2::variant<
      typename basic_http_connection<Executor>::stream,
      typename basic_https_connection<Executor>::stream> impl_;

  friend struct basic_session<Executor>;
};

typedef basic_session<> session;

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
extern template struct basic_session<asio::any_io_executor>;
#endif

}

#include <boost/requests/impl/session.hpp>

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
