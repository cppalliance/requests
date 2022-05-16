// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_CONNECTION_HPP
#define BOOST_REQUESTS_CONNECTION_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/requests/async_semaphore.hpp>
#include <boost/requests/options.hpp>
#include <boost/requests/traits.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/url/url_view.hpp>

namespace boost
{
namespace requests
{

using empty = beast::http::empty_body::value_type;

template<typename Stream>
struct basic_connection
{
    /// The type of the next layer.
    typedef typename std::remove_reference<Stream>::type next_layer_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::executor_type executor_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

    /// Rebinds the socket type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_connection<typename next_layer_type::template rebind_executor<Executor1>::other> other;
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

    /// Construct a stream.
    /**
     * @param arg The argument to be passed to initialise the underlying stream.
     *
     * Everything else will be default constructed

     */

    explicit basic_connection(const executor_type & ex) : next_layer_(ex) {}
    explicit basic_connection(const executor_type & ex, asio::ssl::context& ctx_) : next_layer_(ex, ctx_) {}

    template<typename Arg>
    explicit basic_connection(Arg && arg) : next_layer_(std::forward<Arg>(arg)) {}

    template<typename NextLayer>
    explicit basic_connection(basic_connection<NextLayer> && prev) : next_layer_(std::move(prev.next_layer())) {}


    template <typename ExecutionContext>
    explicit basic_connection(ExecutionContext& context,
                    typename asio::constraint<
                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                    >::type = 0)
            : next_layer_(context)
    {
    }

    template <typename ExecutionContext>
    explicit basic_connection(ExecutionContext& context,
                    asio::ssl::context& ctx_,
                    typename asio::constraint<
                            is_ssl_stream<next_layer_type>::value &&
                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                    >::type = 0)
            : next_layer_(context, ctx_)
    {
    }
    void connect(urls::string_view url)
    {
        connect(urls::parse_uri(url).value());
    }
    void connect(boost::urls::url_view url)
    {
        auto & sock = next_layer_.lowest_layer();

        auto service = url.scheme();
        if (url.has_port())
            service = url.port();
        else if (service == "wss")
            service = "https";
        else if (service == "ws")
            service = "http";

        host = url.encoded_host().to_string();

        // prob can optimize this a bit more with more insight for boost.url
        asio::ip::basic_resolver<typename lowest_layer_type::protocol_type, executor_type> reser{next_layer_.get_executor()};
        auto res = reser.resolve(host, {service.data(), service.size()});
        sock.connect(*res.begin());
        handshake_(next_layer_, is_ssl_stream<next_layer_type>{});
    }
    void connect(urls::string_view url, system::error_code & ec)
    {
        auto uri = urls::parse_uri(url);
        if (uri.error())
            ec = uri.error();
        else
            connect(urls::parse_uri(url).value());
    }

    void connect(boost::urls::url_view url, system::error_code & ec)
    {
        auto & sock = next_layer_.lowest_layer();

        auto service = url.scheme();
        if (url.has_port())
            service = url.port();
        else if (service == "wss")
            service = "https";
        else if (service == "ws")
            service = "http";

        host = url.encoded_host().to_string();
        // prob can optimize this a bit more with more insight for boost.url
        asio::ip::basic_resolver<typename lowest_layer_type::protocol_type, executor_type> reser{next_layer_.get_executor()};
        auto res = reser.resolve(host, {service.data(), service.size()}, ec);
        if (ec)
            return ;
        sock.connect(*res.begin(), ec);
        if (ec)
            return ;
        handshake_(next_layer_, ec, is_ssl_stream<next_layer_type>{});
    }

    template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
         async_connect(urls::string_view url,
                       CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        auto uri = urls::parse_uri(url);
        if (uri.has_error())
            return asio::post(asio::experimental::prepend(std::forward<CompletionToken>(completion_token), uri.error()));
        else
            return async_connect(urls::parse_uri(url).value(), std::forward<CompletionToken>(completion_token));
    }

    template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
        async_connect(boost::urls::url_view url,
                      CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return asio::async_compose<CompletionToken, void(system::error_code)>(
                async_connect_op<asio::associated_allocator_t<CompletionToken>>{
                    this, asio::get_associated_allocator(completion_token), url}, completion_token, next_layer_);
    }

    template<typename ResponseBody = beast::http::string_body,
             typename RequestBody, typename ... Ops>
    auto request(
            beast::http::verb method,
            boost::urls::string_view path,
            const options & opt,
            RequestBody && body,
            Ops && ... ops) -> beast::http::response<ResponseBody>
    {

        using body_type = deduced_body_t<std::decay_t<RequestBody>>;
        beast::http::request<body_type> req{method, path, 11};
        req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
        req.set(beast::http::field::host, host);
        req.body() = std::forward<RequestBody>(body);
        (ops.prepare(req), ...);

    perform_request:
        req.prepare_payload();
        beast::http::write(next_layer_, req);

        beast::flat_buffer buffer;
        beast::http::response<ResponseBody> res;
        beast::http::read(next_layer_, buffer, res);
        if (opt.follow_redirects &&
            to_status_class(res.result()) == beast::http::status_class::redirection)
        {
            auto itr = res.find(beast::http::field::location);
            if (itr != res.end())
            {
                req.target(itr->value());
                goto perform_request;
            }
            asio::detail::throw_error(beast::http::error::bad_field);
        }

        (ops.complete(res), ...);
        return res;
    }

    template<typename ResponseBody = beast::http::string_body,
            typename RequestBody, typename ... Ops>
    auto request(
            beast::http::verb method,
            boost::urls::string_view path,
            const options & opt,
            RequestBody && body,
            system::error_code & ec,
            Ops && ... ops) -> beast::http::response<ResponseBody>
    {

        using body_type = deduced_body_t<std::decay_t<RequestBody>>;
        beast::http::request<body_type> req{method, path, 11};
        req.body() = std::forward<RequestBody>(body);
        req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
        req.set(beast::http::field::host, host);
        (ops.prepare(req), ...);
    perform_request:
        req.prepare_payload();
        beast::http::write(next_layer_, req, ec);
        if (ec)
            return {};

        beast::flat_buffer buffer;
        beast::http::response<ResponseBody> res;
        beast::http::read(next_layer_, buffer, res, ec);
        if (ec)
            return {};
        if (opt.follow_redirects &&
            to_status_class(res.result()) == beast::http::status_class::redirection)
        {
            auto itr = res.find(beast::http::field::location);
            if (itr != res.end())
            {
                req.target(itr->value());
                goto perform_request;
            }
            ec = beast::http::error::bad_field;
            return {};
        }

        (ops.complete(res), ...);
        return res;
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
                  const options & opt,
                  RequestBody && body,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                  Ops && ... ops)
    {
        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        using request_type = beast::http::request<deduced_body_t<std::decay_t<RequestBody>>, beast::http::basic_fields<c_alloc>>;
        auto alloc = asio::get_associated_allocator(completion_token);
        auto req = allocate_unique<request_type>(alloc, method, path, 11, std::forward<RequestBody>(body), c_alloc{alloc});

        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::http::response<ResponseBody>)>(
                async_request_op<asio::associated_allocator_t<CompletionToken>,
                                std::decay_t<ResponseBody>, std::decay_t<RequestBody>, Ops...>{
                       this, alloc, opt,
                       {std::forward<Ops>(ops)...},
                       std::move(req)
                       }, completion_token, next_layer_);
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
                       const options & opt,
                       RequestBody && body,
                       Ops && ... ops)
    {
        using CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type);
        CompletionToken  && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type);

        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        using request_type = beast::http::request<deduced_body_t<std::decay_t<RequestBody>>, beast::http::basic_fields<c_alloc>>;
        auto alloc = asio::get_associated_allocator(completion_token);
        auto req = allocate_unique<request_type>(alloc, method, path, 11, std::forward<RequestBody>(body), c_alloc{alloc});

        return asio::async_compose<CompletionToken,
                void(boost::system::error_code, beast::http::response<ResponseBody>)>(
                async_request_op<asio::associated_allocator_t<CompletionToken>,
                                std::decay_t<ResponseBody>, std::decay_t<RequestBody>, Ops...>{
                       this, alloc, opt,
                       {std::forward<Ops>(ops)...},
                       std::move(req)
                       }, completion_token, next_layer_);
    }

#endif
    //get, head, post, put, delete, connect, options, trace, patch


    template<typename ... Ops>
    auto handshake(
            boost::urls::string_view path,
            const options &,
            Ops && ... ops) && -> beast::websocket::stream<next_layer_type>
    {
        using ws_t = beast::websocket::stream<next_layer_type >;

        ws_t ws{std::move(next_layer_)};
        ws.set_option(
                beast::websocket::stream_base::decorator(
                   [&](beast::websocket::request_type  & req)
                   {
                       req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
                       (ops.prepare(req), ...);
                   }
                ));

        beast::websocket::response_type res;
        ws.handshake(res, host, path);
        return ws;
    }

    template<typename ... Ops>
    auto handshake(
            boost::urls::string_view path,
            const options &,
            system::error_code & ec,
            Ops && ... ops) && -> beast::websocket::stream<next_layer_type>
    {
        using ws_t = beast::websocket::stream<next_layer_type >;
        ws_t ws{std::move(next_layer_)};
        ws.set_option(
                beast::websocket::stream_base::decorator(
                        [&](beast::websocket::request_type  & req)
                        {
                            req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
                            (ops.prepare(req), ...);
                        }
                ));

        beast::websocket::response_type res;
        ws.handshake(res, host, path, ec);
        return ws;
    }

    template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, beast::websocket::stream<next_layer_type>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            typename ... Ops>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, beast::websocket::stream<next_layer_type>))
    async_handshake(boost::urls::string_view url,
                    const options &,
                    CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type),
                    Ops && ... ops) &&
    {
        using allocator = asio::associated_allocator_t<CompletionToken>;
        using c_alloc = typename std::allocator_traits<allocator>::template rebind_alloc<char>;
        auto alloc = asio::get_associated_allocator(completion_token);
        return asio::async_compose<CompletionToken,
                                   void(boost::system::error_code, beast::websocket::stream<next_layer_type>)>(
                async_handshake_op<asio::associated_allocator_t<CompletionToken>, Ops...>{
                    this, alloc, url.to_string(c_alloc(alloc)),
                    {std::forward<Ops>(ops)...}}, completion_token, next_layer_);
    }
  private:
    template<typename Allocator>
    struct async_connect_op
    {
        basic_connection * this_;
        Allocator alloc;
        using char_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;
        boost::urls::url_view url;
        using resolver_type =  asio::ip::basic_resolver<typename lowest_layer_type::protocol_type, executor_type>;
        std::unique_ptr<resolver_type, alloc_deleter<resolver_type, Allocator>> resolver{nullptr, alloc_deleter<resolver_type, Allocator>{alloc}};

        std::basic_string<char, std::char_traits<char>, char_alloc> service{char_alloc{alloc}};
        template<typename Self>
        void operator()(Self && self)
        {
            auto & sock = this_->next_layer_.lowest_layer();

            service = url.scheme().to_string(char_alloc{alloc});
            if (url.has_port())
                service = url.port().to_string(char_alloc{alloc});
            else if (service == "wss")
                service = "https";
            else if (service == "ws")
                service = "http";

            this_->host = url.encoded_host().to_string();

            resolver = allocate_unique<resolver_type>(alloc, this_->next_layer_.get_executor());
            resolver->async_resolve(this_->host, service, std::move(self));
        }

        template<typename Self>
        void operator()(Self && self, system::error_code ec, typename resolver_type::results_type result)
        {
            if (ec)
                return self.complete(ec);
            this_->next_layer_.lowest_layer()
                 .async_connect(*result.begin(),
                                asio::experimental::prepend(std::move(self), is_ssl_stream<next_layer_type>{}));
        }

        template<typename Self>
        void operator()(Self && self, std::false_type, system::error_code ec)
        {
            self.complete(ec);
        }

        template<typename Self>
        void operator()(Self && self, std::true_type, system::error_code ec)
        {
            this_->next_layer_.async_handshake(asio::ssl::stream_base::client, std::move(self));
        }

        template<typename Self>
        void operator()(Self && self, system::error_code ec)
        {
            self.complete(ec);
        }
    };


    template<typename Allocator, typename ResponseBody, typename RequestBody, typename ...Ops>
    struct async_request_op
    {
        basic_connection * this_;
        Allocator alloc;
        options opt;

        using request_body_type = deduced_body_t<std::decay_t<RequestBody>>;
        using c_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;

        using request_type  = beast::http::request<request_body_type, beast::http::basic_fields<c_alloc>>;
        using response_type = beast::http::response<ResponseBody>;

        template<typename T>
        using unique_ptr = std::unique_ptr<T, alloc_deleter<T, Allocator>>;
        std::tuple<Ops...> ops;

        unique_ptr<request_type>  req;
        unique_ptr<response_type> res{nullptr, alloc_deleter<response_type, Allocator>{alloc}};
        unique_ptr<beast::flat_buffer> buffer = allocate_unique<beast::flat_buffer>(alloc);

        struct dispatched_t {};

        template<typename Self>
        void operator()(Self && self)
        {
            asio::dispatch(this_->get_executor(), asio::experimental::prepend(std::move(self), dispatched_t{}));
        }

        template<typename Self>
        void operator()(Self && self, dispatched_t)
        {
            req->set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
            req->set(beast::http::field::host, this_->host);
            std::apply([&](auto && ... ops) {(ops.prepare(*req), ...);}, ops);

            perform_request(std::move(self));
        }

        template<typename Self>
        void perform_request(Self && self)
        {
            req->prepare_payload();
            synchronized(this_->write_sem_,
                         [self = this_, req = req.get()](auto && token)
                         {
                            return beast::http::async_write(self->next_layer_, *req, std::move(token));
                         },
                         asio::experimental::prepend(std::move(self), &*req));
        }

        template<typename Self>
        void operator()(Self && self, request_type * , system::error_code ec, std::size_t)
        {
            if (ec)
                return self.complete(ec, {});

            if (!res)
                res = allocate_unique<response_type>(alloc);

            synchronized(this_->read_sem_,
                         [self = this_, &res = *res, buf = buffer.get()](auto && token)
                         {
                             return beast::http::async_read(self->next_layer_, *buf, res, std::move(token));
                         },
                         asio::experimental::prepend(std::move(self), &*res));
        }
        template<typename Self>
        void operator()(Self && self, response_type * res, system::error_code ec, std::size_t)
        {
            if (!ec &&
                opt.follow_redirects &&
                to_status_class(res->result()) == beast::http::status_class::redirection)
            {
                auto itr = res->find(beast::http::field::location);
                if (itr != res->end())
                {
                    req->target(itr->value());
                    perform_request(std::move(self));
                    return ;
                }
                ec = beast::http::error::bad_field;
            }
            if (!ec)
                std::apply([&](auto && ... ops) {(ops.complete(*res), ...);}, ops);
            self.complete(ec, std::move(*res));
        }
    };

    template<typename Allocator, typename ...Ops>
    struct async_handshake_op
    {
        basic_connection * this_;
        Allocator alloc;
        using char_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;
        std::basic_string<char, std::char_traits<char>, char_alloc> path{char_alloc{alloc}};
        std::tuple<Ops...> ops;
        using ws_t = beast::websocket::stream<next_layer_type >;

        template<typename T>
        using unique_ptr = std::unique_ptr<T, alloc_deleter<T, Allocator>>;

        unique_ptr<ws_t> ws{nullptr, alloc_deleter<ws_t, Allocator>{alloc}};
        unique_ptr<beast::websocket::response_type> res{nullptr, alloc_deleter<beast::websocket::response_type, Allocator>{alloc}};

        struct dispatched_t {};

        template<typename Self>
        void operator()(Self && self)
        {
            asio::dispatch(this_->get_executor(), asio::experimental::prepend(std::move(self), dispatched_t{}));
        }

        template<typename Self>
        void operator()(Self && self, dispatched_t)
        {
            // we need to grab both semaphores, bc we steal the connection
            asio::experimental::make_parallel_group(
                    [this_ = this_](auto token){return this_-> read_sem_.async_acquire(token);},
                    [this_ = this_](auto token){return this_->write_sem_.async_acquire(token);})
                    .async_wait(asio::experimental::wait_for_one_error(),
                                std::move(self));
        }

        template<typename Self>
        void operator()(Self && self, std::array<std::size_t, 2u> /*order*/,
                                      system::error_code ec_read, system::error_code ec_write) //got the semaphores
        {
            if (ec_read)
                return self.complete(ec_read, this_->disconnected_());
            if (ec_write)
                return self.complete(ec_read, this_->disconnected_());

            ws = allocate_unique<ws_t>(alloc, std::move(this_->next_layer_));
            res = allocate_unique<beast::websocket::response_type>(alloc);
            ws->set_option(
                    beast::websocket::stream_base::decorator(
                            [ops = ops](beast::websocket::request_type  & req)
                            {
                                req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING "/request");
                                std::apply([&](auto ... op){ (op.prepare(req), ...); }, ops);
                            }
                    ));

            // we stole the connection, so let's just fire up the semaphores
            this_->write_sem_.release_all();
            this_->read_sem_.release_all();

            ws->async_handshake(*res, this_->host, path, std::move(self));
        }
        template<typename Self>
        void operator()(Self && self, system::error_code ec)
        {
            if (!ec)
                std::apply([&](auto ... op){ (op.complete(*res), ...); }, ops);

            self.complete(ec, std::move(*ws));
        }
    };

    beast:: websocket::stream<next_layer_type> disconnected_()
    {
        return disconnected_(is_ssl_stream<next_layer_type>{});
    }

    template<typename = void>
    beast:: websocket::stream<next_layer_type> disconnected_(std::true_type)
    {
        return beast:: websocket::stream<next_layer_type>{
            next_layer_.get_executor(),
            next_layer_.native_handle()
        };
    }

    template<typename = void>
    beast:: websocket::stream<next_layer_type> disconnected_(std::false_type)
    {
        return beast:: websocket::stream<next_layer_type>{
            next_layer_.get_executor()
        };
    }

    void handshake_(Stream &, std::false_type) {}
    void handshake_(Stream & stream, std::true_type)
    {
        stream.handshake(asio::ssl::stream_base::client);
    }

    void handshake_(Stream &, system::error_code &, std::false_type) {}
    void handshake_(Stream & stream,  system::error_code & ec, std::true_type)
    {
        stream.handshake(asio::ssl::stream_base::client, ec);
    }

    Stream next_layer_;
    basic_async_semaphore<executor_type> read_sem_{asio::make_strand(next_layer_.get_executor()), 1},
                                        write_sem_{asio::make_strand(next_layer_.get_executor()), 1};
    std::string host;
    beast::flat_buffer buffer_;
};

using http_connection  = basic_connection<asio::ip::tcp::socket>;
using https_connection = basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>;

}
}

#endif //BOOST_REQUESTS_CONNECTION_HPP
