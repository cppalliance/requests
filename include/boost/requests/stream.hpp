//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_STREAM_HPP
#define BOOST_REQUESTS_STREAM_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/tracker.hpp>
#include <boost/requests/fields/keep_alive.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/detail/async_coroutine.hpp>
#include <boost/beast/http/basic_parser.hpp>
#include <boost/asio/execution/bad_executor.hpp>
#include <boost/asem/basic_mutex.hpp>
#include <boost/asem/lock_guard.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

struct stream_base
{
  virtual std::size_t do_read_some_(beast::http::basic_parser<false> & parser) = 0;
  virtual std::size_t do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec) = 0;
  virtual void  do_async_read_some_(beast::http::basic_parser<false> & parser, detail::co_token_t<void(system::error_code, std::size_t)>) = 0;

  virtual void do_close_(system::error_code & ec) = 0;
  virtual void do_async_close_(detail::co_token_t<void(system::error_code)>) = 0;

  virtual keep_alive & get_keep_alive_set_() = 0;
  virtual bool is_open() const = 0;
};

struct pmr_deleter
{
  container::pmr::memory_resource * res;
  constexpr pmr_deleter(container::pmr::memory_resource * res = container::pmr::get_default_resource()) noexcept : res(res) {}

  template<typename T>
  void operator()(T * ptr)
  {
    ptr->~T();
    res->deallocate(ptr, sizeof(T), alignof(T));
  }
};

template<typename T, typename ... Args>
std::unique_ptr<T, pmr_deleter> make_pmr(container::pmr::memory_resource * res, Args && ... args)
{
  void * raw = res->allocate(sizeof(T), alignof(T));
  try
  {
    return {new (raw) T(std::forward<Args>(args)...), res};

  }
  catch (...)
  {
    res->deallocate(raw, sizeof(T), alignof(T));
    throw;
  }
}

}

template<typename Executor = asio::any_io_executor>
struct basic_stream
{
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  /// Get the executor
  executor_type get_executor() noexcept
  {
    return executor_;
  }

  /// Check if the underlying connection is open.
  bool is_open() const
  {
    return impl_ && impl_->is_open() && !done();
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer)
  {
    boost::system::error_code ec;
    auto res = read_some(buffer, ec);
    if (ec)
      throw_exception(system::system_error(ec, "read_some"));
    return res;
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer, system::error_code & ec);

  /// Read some data from the request body.
  template<
      typename MutableBufferSequence,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken
          BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
  async_read_some(
      const MutableBufferSequence & buffers,
      CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

  /// Read all the data from the request body.
  template<typename DynamicBuffer>
  std::size_t read(DynamicBuffer & buffer)
  {
    boost::system::error_code ec;
    auto res = read(buffer, ec);
    if (ec)
      throw_exception(system::system_error(ec, "read"));
    return res;
  }


  /// Read all the data from the request body.
  template<typename DynamicBuffer>
  std::size_t read(DynamicBuffer & buffer, system::error_code & ec);

  /// Read all the data from the request body.
  template<
      typename DynamicBuffer,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken
          BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
  async_read(
      DynamicBuffer & buffers,
      CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));


  /// dump the rest of the data.
  void dump()
  {
    boost::system::error_code ec;
    dump(ec);
    if (ec)
      throw_exception(system::system_error(ec, "dump"));
  }
  void dump(system::error_code & ec);

  /// Read some data from the request body.
  template<
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
          CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
  async_dump(CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

  basic_stream(basic_stream && lhs) = default;
  basic_stream& operator=(basic_stream && lhs) = default;

  basic_stream           (const basic_stream &) = delete;
  basic_stream& operator=(const basic_stream &) = delete;
  ~basic_stream();

  using history_type = response_base::history_type;

  const http::response_header &headers() const &
  {
    if (!parser_)
    {
      static http::response_header h;
      return h;
    }
    return parser_->get().base();
  }
  const history_type          &history() const & { return history_; }


  bool done() const {return !parser_ ||  parser_->is_done();}
  explicit basic_stream(executor_type executor, std::nullptr_t ) : executor_{executor}, impl_(nullptr) {}
  explicit basic_stream(executor_type executor,
                        detail::stream_base * impl,
                        detail::tracker t = {})
      : executor_{executor},
        impl_(impl),
        t_(std::move(t)) {}

  http::response_header &&headers() &&
  {
    if (!parser_)
    {
      static http::response_header h;
      return std::move(h);
    }

    return std::move(parser_->get().base());
  }
  history_type          &&history() && { return std::move(history_); }


  void prepend_history(history_type && pre_history)
  {
    history_.insert(history_.begin(),
                    std::make_move_iterator(history_.begin()),
                    std::make_move_iterator(history_.end()));
  }
 private:
  executor_type executor_;
  detail::stream_base* impl_;
  asem::lock_guard<detail::basic_mutex<executor_type>> lock_;

  std::unique_ptr<http::response_parser<http::buffer_body>,
                  detail::pmr_deleter> parser_;
  history_type history_;
  detail::tracker t_;

  template<typename DynamicBuffer>
  struct async_read_op;
  struct async_dump_op;
  struct async_read_some_op;

  template<typename>
  friend struct basic_connection;
};

using stream = basic_stream<>;

}
}

#include <boost/requests/impl/stream.hpp>

#endif // BOOST_REQUESTS_STREAM_HPP
