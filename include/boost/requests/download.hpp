//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DOWNLOAD_HPP
#define BOOST_REQUESTS_DOWNLOAD_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/service.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/url/url_view.hpp>

#if defined(BOOST_ASIO_HAS_FILE)
#include <boost/asio/basic_stream_file.hpp>
#else
#include "request.hpp"
#include <boost/beast/core/file.hpp>
#endif


namespace boost
{
namespace requests
{

#if defined(BOOST_ASIO_HAS_FILE)

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file, system::error_code & ec)
{
  asio::basic_stream_file<typename std::decay_t<Stream>::executor_type> f{str.get_executor()};
  f.open(file.string().c_str(), asio::file_base::write_only | asio::file_base::create, ec);
  if (ec)
    return 0u;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0;

  while (!str.done() && !ec)
  {
    system::error_code ec_read;
    std::size_t n = str.read_some(asio::buffer(buffer), ec_read);

    if (n == 0 && ec_read)
    {
      ec = ec_read;
      return written;
    }

    written += f.write_some(asio::buffer(buffer, n), ec);

    if (ec_read && !ec)
      ec = ec_read;
  }

  return written;
}


namespace detail
{


template<typename Stream>
struct async_write_to_file_op : asio::coroutine
{
  constexpr static const char * op_name = "async_write_to_file_op";

  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return state->str.get_executor(); }

  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  const filesystem::path & file;

  struct state_t
  {
    state_t(Stream * str) : str(*str)) {}

    Stream & str;
    asio::basic_stream_file<typename std::decay_t<Stream>::executor_type>  f{str.get_executor()};
    char buffer[BOOST_REQUESTS_CHUNK_SIZE];

    std::size_t written = 0u;
    system::error_code ec_read;
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  template<typename Stream_>
  async_write_to_file_op(allocator_type alloc, Stream_ * str, const filesystem::path & pt)
      : file(pt), state(allocate_unique<state_t>(alloc, str))
  {
  }

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t n = 0u)
  {
    auto st = state.get();
    BOOST_ASIO_CORO_REENTER(this)
    {
      st->f.open(file.string().c_str(), asio::file_base::write_only | asio::file_base::create, ec);
      if (ec)
      {
        st->written = 0u;
        break;
      }

      while (!st->str.done() && !ec)
      {
        // KDM: this could be in parallel to write using parallel_group.
        BOOST_REQUESTS_YIELD {
          auto b = asio::buffer(st->buffer);
          st->str.async_read_some(b, std::move(self));
        }

        if (n == 0 && ec)
          break;

        st->ec_read = exchange(ec, {});
        BOOST_REQUESTS_YIELD asio::async_write(st->f, asio::buffer(st->buffer, n), std::move(self));

        st->written += n;
        if (st->ec_read && !ec)
          ec = st->ec_read;
      }
    }
    if (is_complete())
    {
        state.reset();
        self.complete(ec, st->written);
    }
  }
};

}

#else

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file, system::error_code & ec)
{
  beast::file f;
  f.open(file.string().c_str(), beast::file_mode::write_new, ec);
  if (ec)
    return 0u;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0;

  while (!str.done() && !ec)
  {
    system::error_code ec_read;
    std::size_t n = str.read_some(asio::buffer(buffer), ec_read);

    if (n == 0 && ec_read)
    {
      ec = ec_read;
      return written;
    }

    written += f.write(buffer, n, ec);

    if (ec_read && !ec)
      ec = ec_read;
  }

  return written;
}

namespace detail
{


template<typename Stream>
struct async_write_to_file_op : asio::coroutine
{
  constexpr static const char * op_name = "async_write_to_file_op";

  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return state->str.get_executor(); }

  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  const filesystem::path & file;

  struct state_t
  {
    state_t(Stream * str) : str(*str) {}
    Stream &str;
    beast::file f;
    char buffer[BOOST_REQUESTS_CHUNK_SIZE];

    std::size_t written = 0u;
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, std::size_t)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;


  async_write_to_file_op(allocator_type alloc, Stream * str, const filesystem::path & pt)
        : file(pt), state(allocate_unique<state_t>(alloc, str))  {}

  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec = {}, std::size_t n = 0u)
  {
    auto st = state.get();
    BOOST_ASIO_CORO_REENTER(this)
    {
      state->f.open(file.string().c_str(), beast::file_mode::write_new, ec);
      if (ec)
      {
        state->written = 0u;
        BOOST_REQUESTS_YIELD {
          auto exec = asio::get_associated_immediate_executor(self, state->str.get_executor());
          asio::dispatch(exec, std::move(self));
        }
        break;
      }

      while (!ec && !state->str.done())
      {
        BOOST_REQUESTS_YIELD state->str.async_read_some(asio::buffer(state->buffer), std::move(self));
        if (n == 0 && ec)
          break;

        system::error_code ec_read = exchange(ec, {});
        state->written += state->f.write(state->buffer, n, ec);

        if (ec_read && !ec)
          ec = ec_read;
      }
    }
    if (is_complete())
    {
      state.reset();
      self.complete(ec, st->written);
    }
  }
};

template<typename Stream>
void async_write_to_file_impl(
    asio::any_completion_handler<void(boost::system::error_code, std::size_t)> handler,
    Stream * str, const filesystem::path & file)
{
  return asio::async_compose<asio::any_completion_handler<void(boost::system::error_code, std::size_t)>,
                             void(boost::system::error_code, std::size_t)>(
      detail::async_write_to_file_op<Stream>{asio::get_associated_allocator(handler), str, file},
      handler, str->get_executor());
}

}



#endif

template<typename Stream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::size_t)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, std::size_t))
async_write_to_file(Stream & str, const filesystem::path & file,
                    CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, std::size_t)>(
      &detail::async_write_to_file_impl<Stream>,
      completion_token, &str, file);
}

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file)
{
  boost::system::error_code ec;
  auto res = write_to_file(std::forward<Stream>(str), file, ec);
  if (ec)
    throw_exception(system::system_error(ec, "write_to_file"));
  return res;
}

struct download_response : response_base
{
  filesystem::path download_path;

  download_response() = default;

  download_response(http::response_header header, response_base::history_type history, filesystem::path file) : response_base(std::move(header), std::move(history)), download_path(std::move(file)) {}
  download_response(response_base         header, filesystem::path file) : response_base(std::move(header)), download_path(std::move(file)) {}

  download_response(const download_response & ) = default;
  download_response(download_response && ) noexcept = default;

  download_response& operator=(const download_response & ) = default;
  download_response& operator=(download_response && ) noexcept = default;
};


template<typename Connection>
inline auto download(Connection & conn,
                     detail::target_view<Connection> target,
                     detail::request_type<Connection> req,
                     filesystem::path download_path,
                     system::error_code & ec) -> download_response
{
  auto rh = request_stream(conn, http::verb::get, target, empty{}, std::move(req), ec);
  auto & ro = rh.first;
  if (ec)
    return download_response{std::move(ro).headers(), std::move(rh.second), {}};

  if (filesystem::exists(download_path, ec) && filesystem::is_directory(download_path, ec) && !target.empty())
    download_path /= target.segments().back(); // so we can download to a folder
  ec.clear();
  if (!ec)
    write_to_file(ro, download_path, ec);
  return download_response{std::move(ro).headers(), std::move(rh.second), std::move(download_path)};
}


template<typename Connection>
inline auto download(Connection & conn,
                     urls::url_view target,
                     detail::request_type<Connection> req,
                     filesystem::path download_path) -> download_response
{
  boost::system::error_code ec;
  auto res = download(conn, target, std::move(req), std::move(download_path), ec);
  if (ec)
    throw_exception(system::system_error(ec, "download"));
  return res;
}

inline auto download(urls::url_view path,
                     http::fields req,
                     filesystem::path download_path,
                     system::error_code & ec) -> download_response
{
  return download(default_session(), path, std::move(req), std::move(download_path), ec);
}


inline auto download(urls::url_view path,
                     http::fields req,
                     filesystem::path download_path) -> download_response
{
  return download(default_session(), path, std::move(req), std::move(download_path));
}


namespace detail
{

template<typename Connection>
struct async_download_op : asio::coroutine
{

  constexpr static const char * op_name = "async_download_op";
  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return state->str.get_executor(); }


  struct state_t
  {
    Connection & conn;
    urls::url_view target;
    detail::request_type<Connection> req;
    filesystem::path download_path;

    download_response rb{};
    optional<stream> str_;

    state_t(Connection * conn,
            urls::url_view target,
            detail::request_type<Connection> req,
            filesystem::path download_path)
        : conn(*conn), target(target), req(std::move(req)), download_path(std::move(download_path)) {}
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, download_response)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;


  async_download_op(allocator_type alloc, Connection * conn,
                    urls::url_view target,
                    detail::request_type<Connection> req,
                    filesystem::path download_path)
      : state(allocate_unique<state_t>(alloc, conn, target, req, std::move(download_path)))
  {}

  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec = {},
                  variant2::variant<variant2::monostate, stream, std::size_t> s = variant2::monostate{},
                  history hist = {})
  {
    auto st = state.get();
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD async_request_stream(st->conn, http::verb::get, st->target, empty{},
                                                std::move(st->req), std::move(self));
      st->rb.history = std::move(hist);
      if (ec)
      {
        st->rb.headers = std::move(variant2::get<1>(s)).headers();
        break;
      }
      st->str_.emplace(std::move(variant2::get<1>(s)));
      if (filesystem::exists(st->download_path, ec) && filesystem::is_directory(st->download_path, ec) && !st->target.segments().empty())
        st->rb.download_path = st->download_path / st->target.segments().back(); // so we can download to a folder
      else
        st->rb.download_path = std::move(st->download_path);
      ec.clear();
      if (!ec)
      {
        BOOST_REQUESTS_YIELD async_write_to_file(*st->str_, st->rb.download_path,
                          asio::deferred([](system::error_code ec, std::size_t){return asio::deferred.values(ec);}))
                (std::move(self));
      }

      st->rb.headers = std::move(*st->str_).headers();
    }
    if (is_complete())
    {
        auto rr = std::move(st->rb);
        state.reset();
        self.complete(ec, std::move(rr));
    }
  }
};

template<typename Connection>
void async_download_impl(
    asio::any_completion_handler<void (boost::system::error_code, download_response)> handler,
    Connection * conn, urls::url_view target, detail::request_type<Connection> req, filesystem::path download_path)
{
  return asio::async_compose<asio::any_completion_handler<void (boost::system::error_code, download_response)>,
                             void(boost::system::error_code, download_response)>(
      detail::async_download_op<Connection>{
          asio::get_associated_allocator(handler),
          conn, target, std::move(req), std::move(download_path)},
      handler, conn->get_executor());
}

}


template<typename Connection,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, download_response)) CompletionToken
              = typename Connection::default_token>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, download_response))
async_download(Connection & conn,
               urls::url_view target,
               detail::request_type<Connection> req,
               filesystem::path download_path,
               CompletionToken && completion_token = typename Connection::default_token())
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, download_response)>(
      &detail::async_download_impl<Connection>, completion_token,
      &conn, target, std::move(req), std::move(download_path));
}

namespace detail
{

struct async_download_op_free
{
  template<typename Handler>
  void operator()(Handler && handler,
                  urls::url_view path,
                  http::fields req,
                  filesystem::path download_path)
  {
    return async_download(default_session(asio::get_associated_executor(handler)),
                          path, std::move(req), std::move(download_path), std::move(handler));
  }
};

}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, download_response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, download_response))
async_download(urls::url_view path,
               http::fields req,
               filesystem::path download_path,
               CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void (boost::system::error_code, download_response)>(
      detail::async_download_op_free{},
      completion_token, path, std::move(req), std::move(download_path));
}


}
}


#endif // BOOST_REQUESTS_DOWNLOAD_HPP
