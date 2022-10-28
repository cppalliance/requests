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

          struct options & options()       {return options_;}
    const struct options & options() const {return options_;}

    template<typename Allocator>
    using basic_request = beast::http::basic_fields<Allocator>;



    template<typename RequestBody, typename Allocator = std::allocator<char>>
    auto request(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 basic_request<Allocator> req,
                 system::error_code & ec) -> basic_response<Allocator>;

    template<typename RequestBody, typename Allocator = std::allocator<char>>
    auto request(beast::http::verb method,
                 urls::url_view path,
                 RequestBody && body,
                 basic_request<Allocator> req)
        -> basic_response<Allocator>
    {
      boost::system::error_code ec;
      auto res = request(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "request"));
      return res;
    }

    template<typename RequestBody,
              typename Allocator = std::allocator<char>,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   basic_response<Allocator>)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            basic_response<Allocator>))
    async_request(beast::http::verb method,
                  urls::url_view path,
                  RequestBody && body,
                  basic_request<Allocator> req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename Allocator = std::allocator<char>>
    auto download(urls::url_view path,
                  basic_request<Allocator> req,
                  const filesystem::path & download_path,
                  system::error_code & ec) -> basic_response<Allocator>;

    template<typename Allocator= std::allocator<char> >
    auto download(urls::url_view path,
                  basic_request<Allocator> req,
                  const filesystem::path & download_path) -> basic_response<Allocator>
    {
      boost::system::error_code ec;
      auto res = download(path, std::move(req), download_path, ec);
      if (ec)
        throw_exception(system::system_error(ec, "request"));
      return res;
    }

    template<typename Allocator = std::allocator<char>,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                                   basic_response<Allocator>)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code,
                                            basic_response<Allocator>))
    async_download(urls::url_view path,
                   basic_request<Allocator> req,
                   filesystem::path download_path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    using target_view = urls::url_view;
#include <boost/requests/detail/alias.def>

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

    template<typename Allocator = std::allocator<char>,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, pool_ptr)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
    async_get_pool(urls::url_view path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

  private:
    asio::ssl::context sslctx_{asio::ssl::context_base::tls_client};
    detail::basic_mutex<executor_type> mutex_;

    struct options options_;

    using host_ = std::pair<
        std::basic_string<char, std::char_traits<char>, container::pmr::polymorphic_allocator<char>>, unsigned short>;

    boost::unordered_map<host_, std::shared_ptr<basic_http_connection_pool<Executor>>> http_pools_;
    boost::unordered_map<host_, std::shared_ptr<basic_https_connection_pool<Executor>>> https_pools_;

    // this isn't great
    boost::container::pmr::synchronized_pool_resource pmr_;
    pmr::cookie_jar jar_{boost::container::pmr::polymorphic_allocator<char>(&pmr_)};

    template<typename RequestBody, typename Allocator>
    struct async_request_op;

    template<typename Allocator>
    struct async_download_op;

    struct async_get_pool_op;

    template<typename Allocator>
    auto make_request_(beast::http::basic_fields<Allocator> fields) -> requests::basic_request<Allocator>;

    auto make_request_(beast::http::fields fields) -> requests::request;

    template<typename>
    friend struct basic_session;
};

typedef basic_session<> session;

#if !defined(BOOST_REQUESTS_HEADER_ONLY)
extern template struct basic_session<asio::any_io_executor>;
#endif

}

#include <boost/requests/impl/session.hpp>

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
