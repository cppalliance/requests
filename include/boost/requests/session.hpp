// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_BASIC_SESSION_HPP
#define BOOST_REQUESTS_BASIC_SESSION_HPP

#include <boost/requests/connection_pool.hpp>


namespace boost
{
namespace requests
{
namespace detail
{
struct url_hash
{
  std::size_t operator()( urls::url_view url ) const
  {
    return boost::hash<core::string_view>()( url.buffer() );
  }
};

}

struct session
{
    /// The type of the executor associated with the object.
    typedef asio::any_io_executor executor_type;

    /// This type with a defaulted completion token.
    template<typename Token>
    struct defaulted;

    /// Rebinds the socket type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
      /// The socket type when rebound to the specified executor.
      using other = session;
    };

    /// Constructor.
    explicit session(const executor_type &ex) : executor_(ex)
    {
      sslctx_.set_default_verify_paths();
    }

    /// Constructor.
    template<typename ExecutionContext>
    explicit session(ExecutionContext &context,
                     typename asio::constraint<
                                   asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                           >::type = 0) : executor_(context.get_executor())
    {
      sslctx_.set_default_verify_paths();
    }

    /// Get the executor associated with the object.
    executor_type get_executor() BOOST_ASIO_NOEXCEPT
    {
        return executor_;
    }

          struct request_options & options()       {return options_;}
    const struct request_options & options() const {return options_;}

    // possibly make it a distinct return type.
    BOOST_REQUESTS_DECL std::shared_ptr<connection_pool>
                                     get_pool(urls::url_view url, error_code & ec);
    std::shared_ptr<connection_pool> get_pool(urls::url_view url)
    {
      boost::system::error_code ec;
      auto res = get_pool(url, ec);
      if (ec)
        throw_exception(system::system_error(ec, "get_pool"));
      return res;
    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, std::shared_ptr<connection_pool>))
    async_get_pool(urls::url_view path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    void shutdown()
    {
      pools_.clear();
    }
          cookie_jar & jar()       {return jar_;};
    const cookie_jar & jar() const {return jar_;};

  private:
    asio::ssl::context sslctx_{asio::ssl::context_base::tlsv12_client};
    asio::any_io_executor executor_;
    std::mutex mutex_;

    struct request_options options_{default_options()};

    boost::unordered_map<urls::url,
                         std::shared_ptr<connection_pool>,
                         detail::url_hash> pools_;

    cookie_jar jar_{};

    struct async_get_pool_op;

    BOOST_REQUESTS_DECL
    static void async_get_pool_impl(
        asio::any_completion_handler<void (boost::system::error_code, std::shared_ptr<connection_pool>)> handler,
        session * sess, urls::url_view url);



    BOOST_REQUESTS_DECL auto make_request_(http::fields fields) -> requests::request_parameters;
    BOOST_REQUESTS_DECL static urls::url normalize_(urls::url_view in);
};

template<typename Token>
struct session::defaulted : session
{
  using default_token = Token;
  using session::session;

    template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, typename detail::defaulted_helper<connection>, Token>::type))
  async_get_pool(CompletionToken && completion_token)
  {
      return session::async_get_pool(detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  auto async_get_pool(urls::url_view path)
  {
    return this->async_get_pool(path, default_token());
  };

  using session::async_get_pool;
};


}
}

#include <boost/requests/impl/session.hpp>

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
