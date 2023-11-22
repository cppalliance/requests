// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_CONNECTION_POOL_HPP

#include <boost/requests/connection.hpp>
#include <boost/requests/detail/condition_variable.hpp>

namespace boost {
namespace requests {

namespace detail
{

struct connection_impl;

struct endpoint_hash
{
  std::size_t operator()(const asio::generic::stream_protocol::endpoint & be) const
  {
    return hash_range(reinterpret_cast<const char*>(be.data()), // yuk
                      reinterpret_cast<const char*>(be.data()) + be.size());
  }
};

}

struct connection_pool
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
        using other = connection_pool;
    };

    /// Get the executor
    executor_type get_executor() noexcept
    {
        return cv_.get_executor();
    }

    /// The protocol-type of the lowest layer.
    using protocol_type = asio::generic::stream_protocol;

    /// The endpoint of the lowest lowest layer.
    using endpoint_type = typename protocol_type::endpoint;

    /// Construct a stream.
    /**
     * @param exec The executor or execution_context.
     *
     * Everything else will be default constructed
     */
    explicit connection_pool(asio::any_io_executor exec,
                             std::size_t limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(
              asio::use_service<detail::ssl_context_service>(
                  asio::query(exec, asio::execution::context)
              ).get()), cv_(exec), limit_(limit) {}

    template<typename ExecutionContext>
    explicit connection_pool(ExecutionContext &context,
                             typename asio::constraint<
                                 asio::is_convertible<ExecutionContext &, asio::execution_context &>::value,
                                 std::size_t
                             >::type limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(
              asio::use_service<detail::ssl_context_service>(context).get()),
              cv_(context), limit_(limit) {}

    /// Construct a stream.
    /**
     * @param exec The executor or execution_context.
     *
     * Everything else will be default constructed
     */
    template<typename Exec>
    explicit connection_pool(Exec && exec,
                             asio::ssl::context & ctx,
                             std::size_t limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(ctx), cv_(std::forward<Exec>(exec)), limit_(limit) {}

    /// Move constructor
    BOOST_REQUESTS_DECL
    connection_pool(connection_pool && lhs) ;

    BOOST_REQUESTS_DECL
    ~connection_pool();
    void lookup(urls::url_view av)
    {
      boost::system::error_code ec;
      lookup(av, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }
    BOOST_REQUESTS_DECL void lookup(urls::url_view sv, system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_lookup(urls::url_view av,
                 CompletionToken && completion_token );

    std::size_t limit()  const {return limit_;}
    std::size_t active() const {return conns_.size();}
    std::size_t free()   const {return free_conns_.size();}

    BOOST_REQUESTS_DECL connection borrow_connection(error_code & ec);
    connection borrow_connection()
    {
      boost::system::error_code ec;
      auto res = borrow_connection(ec);
      if (ec)
        throw_exception(system::system_error(ec, "borrow_connection"));
      return res;
    }

    BOOST_REQUESTS_DECL connection steal_connection(error_code & ec);
    connection steal_connection()
    {
      boost::system::error_code ec;
      auto res = steal_connection(ec);
      if (ec)
        throw_exception(system::system_error(ec, "steal_connection"));
      return res;
    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
      async_borrow_connection(CompletionToken && completion_token);


    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
        async_steal_connection(CompletionToken && completion_token);

    bool uses_ssl() const {return use_ssl_;}

    BOOST_REQUESTS_DECL
    void return_connection(connection conn);

    BOOST_REQUESTS_DECL
    void remove_connection(const connection &conn);

    const std::vector<endpoint_type> & endpoints() const { return endpoints_; };

  private:
    bool use_ssl_{true};
    asio::ssl::context & context_;
    std::mutex mtx_;
    detail::condition_variable cv_;
    std::string host_;
    std::vector<endpoint_type> endpoints_;
    std::size_t limit_;

    boost::unordered_multimap<endpoint_type,
                              std::shared_ptr<detail::connection_impl>,
                              detail::endpoint_hash> conns_;

    std::vector<std::shared_ptr<detail::connection_impl>> free_conns_;

    struct async_lookup_op;
    template<bool Steal>
    struct async_get_connection_op;


    friend struct connection;
    friend struct stream;
    friend struct detail::connection_impl;

    BOOST_REQUESTS_DECL
    static void async_borrow_connection_impl(
                                 asio::any_completion_handler<void(error_code, connection)> handler,
                                 connection_pool * this_);


    BOOST_REQUESTS_DECL
    static void async_steal_connection_impl(
                                 asio::any_completion_handler<void(error_code, connection)> handler,
                                 connection_pool * this_);

    BOOST_REQUESTS_DECL
    static void async_lookup_impl(
        asio::any_completion_handler<void(error_code)> handler,
        connection_pool * this_, urls::url_view av);

};

template<typename Token>
struct connection_pool::defaulted : connection_pool
{
  using default_token = Token;
  using connection_pool::connection_pool;
  using stream     = typename requests::stream::defaulted<Token>;
  using connection = typename requests::connection::defaulted<Token>;

  using connection_pool::async_lookup;

  auto async_lookup(urls::url_view av)
  {
    return connection_pool::async_lookup(av, default_token());
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, typename detail::defaulted_helper<connection>, Token>::type))
  async_borrow_connection(CompletionToken && completion_token)
  {
    return connection_pool::async_borrow_connection(detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, typename detail::defaulted_helper<connection, Token>::type))
  async_steal_connection(CompletionToken && completion_token)
  {
    return connection_pool::async_steal_connection(detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  auto async_borrow_connection()
  {
    return this->async_borrow_connection(default_token());
  }

  auto async_steal_connection()
  {
    return this->async_steal_connection(default_token());
  }

};


}
}

#include <boost/requests/impl/connection_pool.hpp>

#endif //BOOST_REQUESTS_CONNECTION_POOL_HPP
