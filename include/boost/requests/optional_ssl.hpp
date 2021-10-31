// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_OPTIONAL_SSL_HPP
#define BOOST_REQUESTS_OPTIONAL_SSL_HPP

#include <boost/asio/experimental/deferred.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace boost::requests
{

template<typename Executor = asio::any_io_executor>
struct basic_optional_ssl
{
    /// The type of the executor associated with the object.
    typedef Executor executor_type;

    /// Rebinds the socket type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_optional_ssl<Executor1> other;
    };

    /// The native representation of a socket.
#if defined(GENERATING_DOCUMENTATION)
    typedef implementation_defined native_handle_type;
#else
    typedef typename asio::basic_stream_socket<asio::ip::tcp, Executor>::native_handle_type native_handle_type;
#endif

    /// The protocol type.
    typedef asio::ip::tcp protocol_type;

    /// The endpoint type.
    typedef typename protocol_type::endpoint endpoint_type;

    typedef typename asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>>::handshake_type handshake_type;

    /// Construct a basic_stream_socket without opening it.
    /**
     * This constructor creates a stream socket without opening it. The socket
     * needs to be opened and then connected or accepted before data can be sent
     * or received on it.
     *
     * @param ex The I/O executor that the socket will use, by default, to
     * dispatch handlers for any asynchronous operations performed on the socket.
     */
    explicit basic_optional_ssl(const executor_type& ex, asio::ssl::context & ctx) : socket_(ex, ctx)
    {
    }

    /// Construct a basic_stream_socket without opening it.
    /**
     * This constructor creates a stream socket without opening it. The socket
     * needs to be opened and then connected or accepted before data can be sent
     * or received on it.
     *
     * @param context An execution context which provides the I/O executor that
     * the socket will use, by default, to dispatch handlers for any asynchronous
     * operations performed on the socket.
     */
    template <typename ExecutionContext>
    explicit basic_optional_ssl(ExecutionContext& exec_context, asio::ssl::context & ssl_context,
    typename asio::constraint<asio::is_convertible<ExecutionContext&, asio::execution_context&>::value >::type = 0)
        : socket_(exec_context, ssl_context)
    {
    }

    basic_optional_ssl(basic_optional_ssl&& other) BOOST_ASIO_NOEXCEPT  = default;
    basic_optional_ssl& operator=(basic_optional_ssl&& other) BOOST_ASIO_NOEXCEPT  = default;

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        if (is_ssl_)
            return socket_.write_some(buffers);
        else
            return socket_.next_layer().write_some(buffers);
    }


    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers, system::error_code & ec)
    {
        if (is_ssl_)
            return socket_.write_some(buffers, ec);
        else
            return socket_.next_layer().write_some(buffers, ec);
    }

    template <typename ConstBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
          std::size_t)) WriteHandler
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler,
        void (boost::system::error_code, std::size_t))
    async_write_some(const ConstBufferSequence& buffers,
                     BOOST_ASIO_MOVE_ARG(WriteHandler) handler
                     BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        if (is_ssl_)
            return socket_.async_write_some(buffers, BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
        else
            return socket_.next_layer().async_write_some(buffers, BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
    }

    template <typename ConstBufferSequence>
    std::size_t read_some(const ConstBufferSequence& buffers)
    {
        if (is_ssl_)
            return socket_.read_some(buffers);
        else
            return socket_.next_layer().read_some(buffers);
    }


    template <typename ConstBufferSequence>
    std::size_t read_some(const ConstBufferSequence& buffers, system::error_code & ec)
    {
        if (is_ssl_)
            return socket_.read_some(buffers, ec);
        else
            return socket_.next_layer().read_some(buffers, ec);
    }

    void cancel()
    {
        socket_.next_layer().cancel();
    }

    void cancel(system::error_code & ec)
    {
        socket_.next_layer().cancel(ec);
    }

    void close()
    {
        socket_.next_layer().close();
    }

    void close(system::error_code & ec)
    {
        socket_.next_layer().close(ec);
    }

    void shutdown()
    {
        if (is_ssl_)
            socket_.shutdown();
        is_ssl_ = false;
    }
    void shutdown(system::error_code & ec)
    {
        if (is_ssl_)
            socket_.shutdown(ec);
        is_ssl_ = false;
    }

    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
          ShutdownHandler
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ShutdownHandler,
        void (boost::system::error_code))
    async_shutdown(
        BOOST_ASIO_MOVE_ARG(ShutdownHandler) handler
          BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        using asio::experimental::deferred;
        if (is_ssl_)
        {
            return socket_.async_shutdown(deferred)(
                    deferred([this](auto && ec)
                    {
                        if (!ec)
                            is_ssl_ = false;
                        return deferred.values(ec);
                    }))(BOOST_ASIO_MOVE_CAST(ShutdownHandler)(handler));
        }
        else
            return asio::post(socket_.get_executor(), deferred)
                             (deferred([] {return deferred.values(system::error_code{});}))
            (BOOST_ASIO_MOVE_CAST(ShutdownHandler)(handler));
    }

    void handshake(handshake_type type)
    {
        if (!is_ssl_)
            socket_.handshake(type);
        is_ssl_ = true;
    }
    void handshake(handshake_type type,
                   system::error_code & ec)
    {
        if (!is_ssl_)
            socket_.handshake(type, ec);
        is_ssl_ = true;
    }


    template<
            typename ConstBufferSequence>
    void handshake(
            handshake_type type,
            const ConstBufferSequence & buffers)
    {
        if (!is_ssl_)
            socket_.handshake(type, buffers);
        is_ssl_ = true;
    }

    template<
            typename ConstBufferSequence>
    void handshake(
            handshake_type type,
            const ConstBufferSequence & buffers,
            system::error_code & ec)
    {
        if (!is_ssl_)
            socket_.handshake(type, buffers, ec);
        is_ssl_ = true;
    }

    template <
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
            HandshakeHandler
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(HandshakeHandler,
                                       void (boost::system::error_code))
    async_handshake(handshake_type type,
                    BOOST_ASIO_MOVE_ARG(HandshakeHandler) handler
                    BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        using asio::experimental::deferred;
        if (!is_ssl_)
        {
            return socket_.async_handshake(type, deferred)(
                    deferred([this](auto && ec)
                    {
                        if (!ec)
                            is_ssl_ = false;
                        return deferred.values(ec);
                    }))(BOOST_ASIO_MOVE_CAST(HandshakeHandler)(handler));
        }
        else
            return asio::post(socket_.get_executor(), deferred)
                             (deferred([] {return deferred.values(system::error_code{});}))
            (BOOST_ASIO_MOVE_CAST(HandshakeHandler)(handler));
    }


    template <typename ConstBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
          std::size_t)) WriteHandler
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler,
        void (boost::system::error_code, std::size_t))
    async_read_some(const ConstBufferSequence& buffers,
                     BOOST_ASIO_MOVE_ARG(WriteHandler) handler
                     BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        if (is_ssl_)
            return socket_.async_read_some(buffers, BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
        else
            return socket_.next_layer().async_read_some(buffers, BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
    }

    void connect(endpoint_type ep)
    {
        socket_.next_layer().connect(ep);
    }

    void connect(endpoint_type ep, system::error_code & ec)
    {
        socket_.next_layer().connect(ep, ec);
    }

    template <
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
            ConnectHandler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
                                       void (boost::system::error_code))
    async_connect(const endpoint_type& peer_endpoint,
                  BOOST_ASIO_MOVE_ARG(ConnectHandler) handler
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return socket_.next_layer().async_connect(peer_endpoint, BOOST_ASIO_MOVE_CAST(ConnectHandler)(handler));
    }

    typedef asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> layer_type;

          layer_type & layer()       { return socket_; }
    const layer_type & layer() const { return socket_; }

    typedef typename layer_type::next_layer_type next_layer_type;

         next_layer_type & next_layer()       { return socket_.next_layer(); }
    const next_layer_type & next_layer() const { return socket_.next_layer(); }

    typedef typename layer_type::lowest_layer_type lowest_layer_type;

          lowest_layer_type & lowest_layer()       { return socket_.lowest_layer(); }
    const lowest_layer_type & lowest_layer() const { return socket_.lowest_layer(); }

  private:
    asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> socket_;
    bool is_ssl_{false};
};


typedef basic_optional_ssl<> optional_ssl;

}

#endif //BOOST_REQUESTS_OPTIONAL_SSL_HPP
