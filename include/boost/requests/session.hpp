// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_BASIC_SESSION_HPP
#define BOOST_REQUESTS_BASIC_SESSION_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/requests/connection_pool.hpp>
#include <boost/beast/http/message.hpp>

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
    }

    /// Constructor.
    template<typename ExecutionContext>
    explicit basic_session(ExecutionContext &context,
                     typename asio::constraint<
                                   asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                           >::type = 0)
            : mutex_(context.get_executor())
    {
    }

    /// Get the executor associated with the object.
    executor_type get_executor() BOOST_ASIO_NOEXCEPT
    {
        return mutex_.get_executor();
    }


    template<typename RequestBody, typename Allocator = std::allocator<char>>
    auto request(beast::http::verb method,
                 urls::pct_string_view path,
                 RequestBody && body,
                 basic_request<Allocator> req,
                 system::error_code & ec) -> basic_response<Allocator>;

    template<typename RequestBody, typename Allocator = std::allocator<char>>
    auto request(beast::http::verb method,
                 urls::pct_string_view path,
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
                  urls::pct_string_view path,
                  RequestBody && body,
                  basic_request<Allocator> req,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename Allocator = std::allocator<char>>
    auto download(urls::pct_string_view path,
                  basic_request<Allocator> req,
                  const filesystem::path & download_path,
                  system::error_code & ec) -> basic_response<Allocator>;


    template<typename Allocator= std::allocator<char> >
    auto download(urls::pct_string_view path,
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
    async_download(urls::pct_string_view path,
                   basic_request<Allocator> req,
                   filesystem::path download_path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    using target_view = urls::url_view;
#include <boost/requests/detail/alias.def>


  private:
    asio::ssl::context sslctx_{asio::ssl::context_base::tls_client};
    detail::basic_mutex<executor_type> mutex_;

    using host_ = std::pair<std::string, unsigned short>;

    boost::unordered_map<host_, basic_http_connection_pool<Executor>> http_pools_;
    boost::unordered_map<host_, basic_https_connection_pool<Executor>> https_pools_;
};

typedef basic_session<> session;

}

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
