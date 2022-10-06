// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_POOL_HPP
#define BOOST_REQUESTS_POOL_HPP

#include <boost/requests/connection.hpp>
#include <boost/asem/st.hpp>
#include <list>
#include <boost/blank.hpp>

namespace boost::requests
{


template<typename Stream>
struct basic_connection_pool
{
    /// The type of the executor associated with the object.
    typedef typename Stream::executor_type executor_type;

    /// The type of the underlying connection.
    typedef basic_connection<Stream> connection_type;

    /// Rebinds the socket type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_connection_pool<typename Stream::template rebind_executor<Executor1>::other> other;
    };

    /// Get the executor
    executor_type get_executor() noexcept
    {
        return semaphore_.get_executor();
    }


    template<typename T = asio::ssl::context>
    auto ssl() -> std::enable_if_t<is_ssl_stream<Stream>::value, T> &
    {
        return sslctx_;
    }

    template<typename T = asio::ssl::context>
    auto ssl() const -> const std::enable_if_t<is_ssl_stream<Stream>::value, T> &
    {
        return sslctx_;
    }

    BOOST_ASIO_NODISCARD const std::string & host() const {return host_;}
    void set_host(const std::string & host) { host_ = host;}

    explicit basic_connection_pool(const executor_type & ex,
                                   urls::string_view host = "",
                                   std::size_t max_connections = 10)
        : host_(host), semaphore_(ex, max_connections) {}

    explicit basic_connection_pool(const executor_type & ex, asio::ssl::context& ctx_,
                                   urls::string_view host = "", std::size_t max_connections = 10)
        : host_(host), semaphore_(ex, max_connections), sslctx_(ctx_) {}

    template<typename NextLayer>
    explicit basic_connection_pool(basic_connection_pool<NextLayer> && prev)
        : host_(prev.host_), semaphore_(std::move(prev.semaphore_)), sslctx_(prev.sslctx_) {}


    template <typename ExecutionContext>
    explicit basic_connection_pool(ExecutionContext& context,
                                   urls::string_view host = "",
                                   std::size_t max_connections = 10,
                                   typename asio::constraint<
                                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                                   >::type = 0)
                                    :  host_(host), semaphore_(context.get_executor(), max_connections)
    {
    }

    template <typename ExecutionContext>
    explicit basic_connection_pool(ExecutionContext& context,
                                    asio::ssl::context& ctx_,
                                    urls::string_view host = "",
                                    std::size_t max_connections = 10,
                                    typename asio::constraint<
                                            is_ssl_stream<Stream>::value &&
                                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                                    >::type = 0)
    : host_(host),  semaphore_(context.get_executor(), max_connections), sslctx_(ctx_)
    {
    }


    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_request(beast::http::verb method,
                  boost::urls::string_view path,
                  const struct options & opt,
                  RequestBody && body,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                  Ops && ... ops)
    {
        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        using string_type = std::basic_string<char, std::char_traits<char>, c_alloc>;

        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::http::response<ResponseBody>)>(
                async_request_op<asio::associated_allocator_t<CompletionToken>,
                std::decay_t<ResponseBody>, std::decay_t<RequestBody>, Ops...>{
                       this, method, opt,
                       {std::forward<Ops>(ops)...}, std::forward<RequestBody>(body),
                       path.to_string(c_alloc(asio::get_associated_allocator(completion_token)))
               }, completion_token, semaphore_);
    }

    template<typename ResponseBody = beast::http::string_body,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_get(boost::urls::string_view path,
              const struct options & opt,
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
              Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::get,
                                           opt, empty{},
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }


    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<beast::http::empty_body>)) CompletionToken
    BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_head(boost::urls::string_view path,
               const struct options & opt,
               CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
               Ops && ... ops)
    {
        return async_request<beast::http::empty_body>(beast::http::verb::head,
                                                      opt, empty{},
                                                      std::forward<CompletionToken>(completion_token),
                                                      std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_post(   boost::urls::string_view path,
                  const struct options & opt,
                  RequestBody && body,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                  Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::post,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_put(boost::urls::string_view path,
              const struct options & opt,
              RequestBody && body,
              CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
              Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::put,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_delete(boost::urls::string_view path,
                 const struct options & opt,
                 CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                 Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::delete_,
                                           opt, empty{},
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_connect(boost::urls::string_view path,
                  const struct options & opt,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                  Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::connect,
                                           opt, empty{},
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_options(boost::urls::string_view path,
                  const struct options & opt,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                  Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::options,
                                           opt, empty{},
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }


    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<empty>)) CompletionToken
    BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_trace(boost::urls::string_view path,
                const struct options & opt,
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                Ops && ... ops)
    {
        return async_request<empty>(beast::http::verb::trace,
                                    opt, empty{},
                                    std::forward<CompletionToken>(completion_token),
                                    std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::http::response<ResponseBody>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::http::response<ResponseBody>))
    async_patch(   boost::urls::string_view path,
                   const struct options & opt,
                   RequestBody && body,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                   Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::patch,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<CompletionToken>(completion_token),
                                           std::forward<Ops>(ops)...);
    }

#if defined(BOOST_ASIO_HAS_CONCEPTS)

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            typename ... Ops>
    requires (
            (!asio::completion_token_for<Ops, boost::system::error_code, beast::http::response<ResponseBody>> && ...)
            && (sizeof...(Ops) > 0))
    auto async_request(beast::http::verb method,
                       boost::urls::string_view path,
                       const struct options & opt,
                       RequestBody && body,
                       Ops && ... ops)
    {
        using CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type);
        CompletionToken  && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type);

        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        using string_type = std::basic_string<char, std::char_traits<char>, c_alloc>;

        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::http::response<ResponseBody>)>(
                async_request_op<asio::associated_allocator_t<CompletionToken>,
                std::decay_t<ResponseBody>, std::decay_t<RequestBody>, Ops...>{
                       this, method, opt,
                       {std::forward<Ops>(ops)...}, std::forward<RequestBody>(body),
                       path.to_string(c_alloc(asio::get_associated_allocator(completion_token)))
               }, completion_token, semaphore_);
    }


    template<typename ResponseBody = beast::http::string_body, typename ... Ops>
    auto async_get(boost::urls::string_view path,
                   const struct options & opt,
                   Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::get,
                                           opt, empty{},
                                           std::forward<Ops>(ops)...);
    }


    template<typename ... Ops>
    auto async_head(boost::urls::string_view path,
                    const struct options & opt,
                    Ops && ... ops)
    {
        return async_request<beast::http::empty_body>(beast::http::verb::head,
                                                      opt, empty{},
                                                      std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            typename ... Ops>
    auto async_post(   boost::urls::string_view path,
                       const struct options & opt,
                       RequestBody && body,
                       Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::post,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            typename ... Ops>
    auto async_put(boost::urls::string_view path,
                   const struct options & opt,
                   RequestBody && body,
                   Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::put,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename ... Ops>
    auto async_delete(boost::urls::string_view path,
                      const struct options & opt,
                      Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::delete_,
                                           opt, empty{},
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename ... Ops>
    auto async_connect(boost::urls::string_view path,
                       const struct options & opt,
                       Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::connect,
                                           opt, empty{},
                                           std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename ... Ops>
    auto async_options(boost::urls::string_view path,
                       const struct options & opt,
                       Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::options,
                                           opt, empty{},
                                           std::forward<Ops>(ops)...);
    }


    template<typename ... Ops>
    auto async_trace(boost::urls::string_view path,
                     const struct options & opt,
                     Ops && ... ops)
    {
        return async_request<empty>(beast::http::verb::trace,
                                    opt, empty{},
                                    std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody,
            typename ... Ops>
    auto async_patch(boost::urls::string_view path,
                     const struct options & opt,
                     RequestBody && body,
                     Ops && ... ops)
    {
        return async_request<ResponseBody>(beast::http::verb::patch,
                                           opt, std::forward<RequestBody>(body),
                                           std::forward<Ops>(ops)...);
    }
#endif


    template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::websocket::stream<Stream>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::websocket::stream<next_layer_type>))
    async_handshake(boost::urls::string_view url,
                    const struct options &,
                    CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                    Ops && ... ops)
    {
        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        auto alloc = asio::get_associated_allocator(completion_token);
        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::websocket::stream<Stream>)>(
                async_handshake_op<asio::associated_allocator_t<CompletionToken>, Ops...>{
                       this, alloc, url.to_string(c_alloc(alloc)),
                       {std::forward<Ops>(ops)...}}, completion_token, semaphore_);
    }

#if defined(BOOST_ASIO_HAS_CONCEPTS)


    template<typename ... Ops>
    auto async_handshake(boost::urls::string_view url,
                    const struct options &,
                    Ops && ... ops)
    {
        using CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type);
        CompletionToken  && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type);

        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        auto alloc = asio::get_associated_allocator(completion_token);
        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::websocket::stream<Stream>)>(
                async_handshake_op<asio::associated_allocator_t<CompletionToken>, Ops...>{
                       this, alloc, url.to_string(c_alloc(alloc)),
                       {std::forward<Ops>(ops)...}}, completion_token, semaphore_);
    }
#endif

  private:

    template<typename NextLayer>
    friend struct basic_connection_pool;

    template<typename Allocator, typename ResponseBody, typename RequestBody, typename ...Ops>
    struct async_request_op
    {
        basic_connection_pool *this_;
        beast::http::verb method;
        struct options opt;

        using request_body_type = deduced_body_t<std::decay_t<RequestBody>>;
        using c_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;

        std::tuple<Ops...> ops;
        RequestBody body;
        std::basic_string<char, std::char_traits<char>, c_alloc> path;

        template<typename Self>
        void operator()(Self &&self)
        {
            this_->semaphore_.async_acquire(std::move(self));
        }

        template<typename Self>
        void operator()(Self &&self, system::error_code ec)
        {
            if (ec)
                return self.complete(ec, {});

            auto itr = std::find_if(this_->connections_.begin(),
                                    this_->connections_.end(),
                                    [](const connection_type & conn)
                                    {
                                        return conn.available_for_read();
                                    });
            if (itr != this_->connections_.end())
                std::apply(
                        [&](auto ... op)
                        {
                            return itr->async_request(method, path, opt, std::move(body),
                                                      asio::experimental::append(std::move(self), itr), op...);
                        }, ops);
            else
                add_connection(std::move(self), is_ssl_stream<Stream>{});
        }
        template<typename Self>
        void add_connection(Self && self, std::true_type)
        {
            auto & conn = this_->connections_.emplace_back(this_->semaphore_.get_executor(), this_->sslctx_);
            printf("HOST : %s\n", this_->host_.c_str());
            conn.async_connect_to_host(this_->host_, std::move(self));
        }

        template<typename Self>
        void add_connection(Self && self, std::false_type)
        {
            auto & conn = this_->connections_.emplace_back(this_->semaphore_.get_executor(), this_->sslctx_);
            conn.async_connect_to_host(this_->host_, std::move(self));
        }

        template<typename Self, typename Response>
        void operator()(Self &&self, system::error_code ec, Response && response,
                        typename std::list<connection_type>::iterator itr)
        {
            // retry!
            if (ec == asio::error::eof || ec == asio::error::broken_pipe)
            {
                this_->connections_.erase(itr);
                (*this)(std::move(self), system::error_code{});
            }
            else
            {
                this_->semaphore_.release();
                self.complete(ec, response);
            }
        }
    };

    template<typename Allocator, typename ...Ops>
    struct async_handshake_op
    {
        basic_connection_pool * this_;
        Allocator alloc;
        using char_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;
        std::basic_string<char, std::char_traits<char>, char_alloc> path{char_alloc{alloc}};
        std::tuple<Ops...> ops;

        template<typename Self>
        void operator()(Self && self)
        {
            asio::post(asio::prepend(std::move(self), 42));

        }

        template<typename Self>
        void operator()(Self && self, int)
        {
            // TODO: Implement
            self.complete(asio::error::operation_not_supported, this_->disconnected_());
        }
    };

    beast:: websocket::stream<Stream> disconnected_()
    {
        return disconnected_(is_ssl_stream<Stream>{});
    }

    template<typename = void>
    beast:: websocket::stream<Stream> disconnected_(std::true_type)
    {
        return beast:: websocket::stream<Stream>{
                get_executor(),
                sslctx_
        };
    }

    template<typename = void>
    beast:: websocket::stream<Stream> disconnected_(std::false_type)
    {
        return beast:: websocket::stream<Stream>{
                get_executor()
        };
    }


    std::string host_;
    asem::basic_semaphore<asem::st, executor_type> semaphore_,
                          complete_signal_{semaphore_.get_executor(), 0};
    std::list<connection_type> connections_;
    std::conditional_t<is_ssl_stream<Stream>::value, asio::ssl::context&, blank> sslctx_{};
};


template<typename Executor = asio::any_io_executor>
using basic_http_connection_pool  = basic_connection_pool<asio::basic_stream_socket<asio::ip::tcp, Executor>>;

template<typename Executor = asio::any_io_executor>
using basic_https_connection_pool = basic_connection_pool<asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>>>;


using http_connection_pool  = basic_http_connection_pool<>;
using https_connection_pool = basic_https_connection_pool<>;



}

#endif //BOOST_REQUESTS_POOL_HPP
