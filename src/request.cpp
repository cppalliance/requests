//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/request.hpp>
#include <boost/asio/yield.hpp>

#include <iostream>

namespace boost
{
namespace requests
{
namespace detail
{


std::pair<stream, history>
request_stream_impl(
    connection & conn,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    system::error_code & ec)
{
  response_base::history_type h;
  auto s = conn.ropen(method, path, req.headers, *source, req.jar, ec);

  urls::url base, new_url;

  while (is_redirect(s.headers().result()) && !ec)
  {
    beast::flat_buffer buffer;
    s.read(buffer, ec);
    auto & res = h.emplace_back(std::move(s).headers(), std::move(buffer));

    if (ec)
      break;

    auto loc_itr = res.find(http::field::location);
    if (loc_itr == res.end())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break ;
    }

    const auto proto = conn.endpoint().protocol();

    if ((proto == asio::ip::tcp::v4()) || proto == asio::ip::tcp::v6())
        base.set_scheme(conn.uses_ssl() ? "https" : "http");
    else if (proto == asio::local::stream_protocol())
        base.set_scheme("unix");
    else
        base.set_scheme("unknown");
    base.set_path(path);
    auto ref = urls::parse_uri_reference(loc_itr->value());
    if (ref.has_error())
    {
        ec = ref.error();
        break;
    }
    const auto url = urls::resolve(base, *ref, new_url);
    if (url.has_error())
    {
      ec = url.error();
      break;
    }

    if ((new_url.has_authority() &&
        (conn.host() != new_url.encoded_host() ||
        !same_endpoint_on_host(new_url, conn.endpoint()))) ||
        req.opts.redirect == requests::redirect_mode::none)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
      break ;
    }
    if (--req.opts.max_redirects == 0)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
      break ;
    }

    path = new_url.encoded_resource();
    source->reset();
    s = conn.ropen(method, path, req.headers, *source, req.jar, ec);
  }

  return std::make_pair(std::move(s), std::move(h));
}


std::pair<stream, history>
request_stream_impl(
    connection_pool & pool,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    system::error_code & ec)
{
  auto conn = pool.borrow_connection(ec);
  if (ec)
    return std::pair<stream, history>{stream{pool.get_executor(), nullptr}, history{}};

  auto res = request_stream_impl(conn, method, path, std::move(source), std::move(req), ec);
  pool.return_connection(std::move(conn));
  return res;
}

std::pair<stream, history>
request_stream_impl(
    session & sess,
    http::verb method,
    urls::url_view url,
    source_ptr source,
    http::headers headers,
    system::error_code & ec)
{
    response_base::history_type h;
    auto pool = sess.get_pool(url, ec);
    if (ec)
      return std::pair<stream, history>{stream{sess.get_executor(), nullptr}, history{}};
    auto conn = pool->borrow_connection(ec);
    if (ec)
      return std::pair<stream, history>{stream{sess.get_executor(), nullptr}, history{}};

    auto opts = sess.options();

    const auto is_secure = (url.scheme_id() == urls::scheme::https);
    if (!is_secure && opts.enforce_tls)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
      return std::pair<stream, history>{stream{sess.get_executor(), nullptr}, history{}};
    }

    auto s = conn.ropen(method, url.encoded_path(), headers, *source, &sess.jar(), ec);
    urls::url new_url;

    while (is_redirect(s.headers().result()) && !ec)
    {
      beast::flat_buffer buffer;
      s.read(buffer, ec);
      auto & res = h.emplace_back(std::move(s).headers(), std::move(buffer));

      if (ec)
        break;

      auto loc_itr = res.find(http::field::location);
      if (loc_itr == res.end())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
        break ;
      }

      auto ref = urls::parse_uri_reference(loc_itr->value());
      if (ref.has_error())
      {
        ec = ref.error();
        break;
      }
      const auto r = urls::resolve(url, *ref, new_url);
      if (r.has_error())
      {
        ec = r.error();
        break;
      }

      if (!should_redirect(opts.redirect, url, new_url))
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
        break ;
      }

      if (--opts.max_redirects == 0)
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
        break ;
      }

      // check if we need a new connection.
      if (new_url.scheme()    != url.scheme()
       || new_url.authority() != url.authority())
      {
        pool->return_connection(std::move(conn));
        pool = sess.get_pool(new_url, ec);
        if (!ec)
          conn = pool->borrow_connection(ec);
        if (ec)
          break;
      }
      url = new_url;

      source->reset();
      s = conn.ropen(method, url.encoded_path(), headers, *source, &sess.jar(), ec);
    }

    pool->return_connection(std::move(conn));
    return std::make_pair(std::move(s), std::move(h));
}

struct async_request_stream_op : asio::coroutine
{
    connection & conn;
    struct state_t
    {
      state_t(http::verb method,
              urls::pct_string_view path,
              source_ptr source,
              request_parameters req,
              asio::any_io_executor executor)
          : s{std::move(executor), nullptr},
            method(method), source(std::move(source)), base(path), req(std::move(req))
      {
      }

      stream s;
      source_ptr source;

      response_base::history_type h;
      urls::url base, new_url;

      http::verb method;
      request_parameters req;

      beast::flat_buffer buffer;
    };

    using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, stream, history)>;
    std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

    async_request_stream_op(
        connection & conn,
        http::verb method,
        urls::pct_string_view path,
        source_ptr source,
        request_parameters req,
        allocator_type alloc)
        : conn(conn),
          state(allocate_unique<state_t>(alloc, method, path, std::move(source), std::move(req), conn.get_executor()))
    {}



    template<typename Self>
    void operator()(Self && self,
                    boost::system::error_code ec = {},
                    variant2::variant<variant2::monostate, std::size_t, stream> arg = {})
    {
      reenter(this)
      {
        yield conn.async_ropen(state->method, state->base.encoded_resource(), state->req.headers, *state->source, state->req.jar, std::move(self));
        if (!ec)
          state->s = std::move(variant2::get<2>(arg));

        while (is_redirect(state->s.headers().result()) && !ec)
        {
          yield state->s.async_read(state->buffer, std::move(self));
          {
            auto & res = state->h.emplace_back(std::move(state->s).headers(), std::move(state->buffer));

            if (ec)
              break;

            auto loc_itr = res.find(http::field::location);
            if (loc_itr == res.end())
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
              break ;
            }

            const auto proto = conn.endpoint().protocol();
            if ((proto == asio::ip::tcp::v4()) || proto == asio::ip::tcp::v6())
              state->base.set_scheme(conn.uses_ssl() ? "https" : "http");
            else if (proto == asio::local::stream_protocol())
              state->base.set_scheme("unix");
            else
              state->base.set_scheme("unknown");
            auto ref = urls::parse_uri_reference(loc_itr->value());
            if (ref.has_error())
            {
              ec = ref.error();
              break;
            }
            const auto url = urls::resolve(state->base, *ref, state->new_url);
            if (url.has_error())
            {
              ec = url.error();
              break;
            }

            if (state->new_url.has_authority() &&
                (conn.host() != state->new_url.encoded_host() ||
                !same_endpoint_on_host(state->new_url, conn.endpoint())) ||
                state->req.opts.redirect == requests::redirect_mode::none)
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
              break ;
            }

            if (--state->req.opts.max_redirects == 0)
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
              break ;
            }

            state->base = std::move(state->new_url);
            state->source->reset();
          }
          yield conn.async_ropen(state->method, state->base.encoded_resource(), state->req.headers, *state->source, state->req.jar, std::move(self));
          state->s = std::move(variant2::get<2>(arg));
        }
      }

      if (is_complete())
      {
        auto s = std::move(state->s);
        auto h = std::move(state->h);
        state.reset();
        self.complete(ec, std::move(s), std::move(h));
      }
    }
};


struct async_request_stream_pool_op : asio::coroutine
{
    connection_pool & pool;
    struct state_t
    {
      state_t(http::verb method,
              urls::pct_string_view path,
              source_ptr source,
              request_parameters req,
              asio::any_io_executor executor)
          : c{std::move(executor)},
            method(method), source(std::move(source)), base(path), req(std::move(req))
      {
      }

      connection c;
      stream s{c.get_executor(), nullptr};
      source_ptr source;

      response_base::history_type h;
      urls::url base, new_url;

      http::verb method;
      request_parameters req;

      beast::flat_buffer buffer;
    };

    using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, stream, history)>;
    std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

    async_request_stream_pool_op(
        connection_pool & pool,
        http::verb method,
        urls::pct_string_view path,
        source_ptr source,
        request_parameters req,
        allocator_type alloc)
        : pool(pool),
          state(allocate_unique<state_t>(alloc, method, path, std::move(source), std::move(req), pool.get_executor()))
    {}



    template<typename Self>
    void operator()(Self && self,
                    boost::system::error_code ec = {},
                    variant2::variant<variant2::monostate, std::size_t, stream, connection> arg = {})
    {
      reenter(this)
      {
        yield pool.async_borrow_connection(std::move(self));
        if (ec)
          goto pool_unavailable;
        state->c = variant2::get<3>(std::move(arg));

        yield state->c.async_ropen(state->method, state->base.encoded_resource(), state->req.headers, *state->source, state->req.jar, std::move(self));
        while (is_redirect(state->s.headers().result()))
        {
          yield state->s.async_read(state->buffer, std::move(self));
          {
            auto & res = state->h.emplace_back(std::move(state->s).headers(), std::move(state->buffer));

            if (ec)
              break;

            auto loc_itr = res.find(http::field::location);
            if (loc_itr == res.end())
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
              break ;
            }


            const auto proto = state->c.endpoint().protocol();
            if ((proto == asio::ip::tcp::v4()) || proto == asio::ip::tcp::v6())
              state->base.set_scheme(state->c.uses_ssl() ? "https" : "http");
            else if (proto == asio::local::stream_protocol())
              state->base.set_scheme("unix");
            else
              state->base.set_scheme("unknown");
            auto ref = urls::parse_uri_reference(loc_itr->value());
            if (ref.has_error())
            {
              ec = ref.error();
              break;
            }
            const auto url = urls::resolve(state->base, *ref, state->new_url);
            if (url.has_error())
            {
              ec = url.error();
              break;
            }

            if (state->new_url.has_authority() &&
                state->c.host() == state->new_url.encoded_host() &&
                !same_endpoint_on_host(state->new_url, state->c.endpoint()))
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
              break ;
            }

            if (--state->req.opts.max_redirects == 0)
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
              break ;
            }

            state->base = std::move(state->new_url);
            state->source->reset();
          }
          yield state->c.async_ropen(state->method, state->base.encoded_resource(), state->req.headers, *state->source, state->req.jar, std::move(self));
          state->s = std::move(variant2::get<2>(arg));
        }
      }

      if (is_complete())
      {
        pool.return_connection(std::move(state->c));
       pool_unavailable:
        auto s = std::move(state->s);
        auto h = std::move(state->h);
        state.reset();
        self.complete(ec, std::move(s), std::move(h));
      }
    }
};


struct async_request_stream_session_op : asio::coroutine
{
    session & sess;
    struct state_t
    {
      state_t(http::verb method,
              urls::url_view path,
              source_ptr source,
              request_options opts,
              http::headers headers,
              asio::any_io_executor executor)
          : c{std::move(executor)},
            method(method), url(path), source(std::move(source)), opts(std::move(opts)),
            headers(std::move(headers)) {}

      std::shared_ptr<connection_pool> pool;
      connection c;
      stream s{c.get_executor(), nullptr};
      source_ptr source;

      response_base::history_type h;
      urls::url new_url;

      http::verb method;
      urls::url url;
      request_options opts;
      http::headers headers;

      beast::flat_buffer buffer;
    };

    using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, stream, history)>;
    std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

    async_request_stream_session_op(
        session & sess,
        http::verb method,
        urls::url_view path,
        source_ptr source,
        http::headers headers,
        allocator_type alloc)
        : sess(sess),
          state(allocate_unique<state_t>(alloc, method, path, std::move(source), sess.options(), std::move(headers), sess.get_executor()))
    {}



    template<typename Self>
    void operator()(Self && self,
                    boost::system::error_code ec = {},
                    variant2::variant<variant2::monostate, std::size_t, stream, connection, std::shared_ptr<connection_pool>> arg = {})
    {
      reenter(this)
      {
        yield sess.async_get_pool(state->url, std::move(self));
        if (!ec)
        {
          state->pool = variant2::get<4>(std::move(arg));
          yield state->pool->async_borrow_connection(std::move(self));
        }
        if (ec)
          goto pool_unavailable;
        state->c = variant2::get<3>(std::move(arg));
        yield state->c.async_ropen(state->method, state->url.encoded_resource(),
                                   state->headers, *state->source, &sess.jar(), std::move(self));
        state->s = std::move(variant2::get<2>(arg));

        while (is_redirect(state->s.headers().result()))
        {

          yield state->s.async_read(state->buffer, std::move(self));
          {
            auto & res = state->h.emplace_back(std::move(state->s).headers(), std::move(state->buffer));

            if (ec)
              break;

            auto loc_itr = res.find(http::field::location);
            if (loc_itr == res.end())
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
              break ;
            }

            auto ref = urls::parse_uri_reference(loc_itr->value());
            if (ref.has_error())
            {
              ec = ref.error();
              break;
            }
            const auto r = urls::resolve(state->url, *ref, state->new_url);
            if (r.has_error())
            {
              ec = r.error();
              break;
            }
            if (!should_redirect(state->opts.redirect, state->url, state->new_url))
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
              break ;
            }
            if (--state->opts.max_redirects == 0)
            {
              BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
              break ;
            }

          }

          // check if we need a new connection.
          if (state->new_url.scheme()    != state->url.scheme()
           || state->new_url.authority() != state->url.authority())
          {
            state->pool->return_connection(std::move(state->c));
            yield sess.async_get_pool(state->new_url, std::move(self));
            state->pool = variant2::get<4>(std::move(arg));
            if (!ec)
            {
              yield state->pool->async_borrow_connection(std::move(self));
              state->c = variant2::get<3>(std::move(arg));
            }
            if (ec)
              break;
          }

          state->url = state->new_url;
          state->source->reset();
          yield state->c.async_ropen(state->method, state->url.encoded_resource(),
                                     state->headers, *state->source, &sess.jar(), std::move(self));
          state->s = std::move(variant2::get<2>(arg));

        }
      }

      if (is_complete())
      {
        state->pool->return_connection(std::move(state->c));
      pool_unavailable:
        auto s = std::move(state->s);
        auto h = std::move(state->h);
        state.reset();
        self.complete(ec, std::move(s), std::move(h));
      }
    }
};

void async_request_stream_impl(
    connection & conn,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    asio::any_completion_handler<void(error_code, stream, history)> handler)
{
    return asio::async_compose<asio::any_completion_handler<void(error_code, stream, history)>,
                               void(error_code, stream, history)>(
        async_request_stream_op{
            conn, method, path, std::move(source), std::move(req), asio::get_associated_allocator(handler)},
        handler,
        conn.get_executor());
}

void async_request_stream_impl(
    connection_pool & pool,
    http::verb method,
    urls::pct_string_view path,
    source_ptr source,
    request_parameters req,
    asio::any_completion_handler<void(error_code, stream, history)> handler)
{
    return asio::async_compose<asio::any_completion_handler<void(error_code, stream, history)>,
                               void(error_code, stream, history)>(
        async_request_stream_pool_op{
            pool, method, path, std::move(source), std::move(req), asio::get_associated_allocator(handler)},
        handler,
        pool.get_executor());
}

void async_request_stream_impl(
    session & sess,
    http::verb method,
    urls::url_view path,
    source_ptr source,
    http::headers headers,
    asio::any_completion_handler<void(error_code, stream, history)> handler)
{
    return asio::async_compose<asio::any_completion_handler<void(error_code, stream, history)>,
                               void(error_code, stream, history)>(
        async_request_stream_session_op{
            sess, method, path, std::move(source), std::move(headers), asio::get_associated_allocator(handler)},
        handler,
        sess.get_executor());
}

}
}
}