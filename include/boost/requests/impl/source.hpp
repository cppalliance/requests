// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_IMPL_SOURCE_HPP
#define BOOST_REQUESTS_IMPL_SOURCE_HPP

#include <boost/requests/detail/define.hpp>
#include <boost/requests/source.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/variant2/variant.hpp>

namespace boost
{
namespace requests
{
namespace detail
{


struct fixed_source_body
{
  static std::size_t size(source & src)
  {
    return *src.size();
  }
  using value_type = source&;
  struct writer
  {
    source & src;
    char buf_[BOOST_REQUESTS_CHUNK_SIZE];
    using const_buffers_type = asio::const_buffer;

    template<bool isRequest, class Fields>
    explicit
    writer(beast::http::header<isRequest, Fields> const&,
           source & src) : src(src)
    {
      src.reset();
    }

    void
    init(system::error_code& ec)
    {
      ec = {};
    }

    boost::optional<
        std::pair<const_buffers_type, bool>>
    get(system::error_code& ec)
    {
      const_buffers_type res;
      auto read_some = src.read_some(buf_, sizeof(buf_), ec);
      if (ec)
        return boost::none;

      return std::pair<const_buffers_type, bool>{const_buffers_type{buf_, read_some.first}, read_some.second};
    }
  };
};


struct source_body
{
  struct value_type
  {
    source &source_;
    asio::const_buffer prefetched;
    bool more;
  };

  struct writer
  {
    value_type v;
    char buf_[BOOST_REQUESTS_CHUNK_SIZE];
    using const_buffers_type = asio::const_buffer;

    template<bool isRequest, class Fields>
    explicit
    writer(beast::http::header<isRequest, Fields> const&,
           value_type value) : v(std::move(value))
    {
      value.source_.reset();
    }

    void
    init(system::error_code& ec)
    {
      ec = {};
    }

    boost::optional<
        std::pair<const_buffers_type, bool>>
    get(system::error_code& ec)
    {
      if (v.prefetched.size() > 0)
        return std::pair<const_buffers_type, bool>{exchange(v.prefetched, asio::const_buffer("", 0u)), v.more};
      auto read_some = v.source_.read_some(buf_, sizeof(buf_), ec);

      if (ec)
        return boost::none;
      return std::pair<const_buffers_type, bool>{const_buffers_type{buf_, read_some.first}, read_some.second};
    }
  };
};

}

template<typename Stream>
std::size_t write_request(
    Stream & stream,
    http::verb method,
    core::string_view target,
    http::fields & header,
    source &src,
    system::error_code & ec)
{
  const auto itr = header.find(beast::http::field::content_type);
  if (itr == header.end())
  {
    auto def = src.default_content_type();
    if (!def.empty())
      header.set(beast::http::field::content_type, def);
  }
  src.reset();

  if (auto sz = src.size())
  {
    if (*sz == 0)
    {
      http::request<http::empty_body> req(method, target, 11, http::empty_body::value_type(), std::move(header));
      req.prepare_payload();

      auto n =  beast::http::write(stream, req, ec);
      header = std::move(req.base());
      return n;
    }
    else
    {
      http::request<detail::fixed_source_body> req(method, target, 11, src, std::move(header));
      req.prepare_payload();
      auto n =  beast::http::write(stream, req, ec);
      header = std::move(req.base());
      return n;
    }
  }
  else
  {
    char prebuffer[BOOST_REQUESTS_CHUNK_SIZE];
    auto init = src.read_some(prebuffer, sizeof(prebuffer), ec);
    http::request<detail::source_body> req(method, target, 11,
                                           detail::source_body::value_type{src,
                                                                           asio::const_buffer(prebuffer, init.first),
                                                                           init.second}, std::move(header));

    if (!init.second)
      req.set(beast::http::field::content_length, std::to_string(init.first));
    else
      req.prepare_payload();

    auto n =  beast::http::write(stream, req, ec);
    header = std::move(req.base());
    return n;
  }
}

namespace detail
{

template<typename Stream>
struct async_write_request_op : asio::coroutine
{
  constexpr static const char * op_name = "async_write_request_op";
  using executor_type = typename Stream::executor_type;
  executor_type get_executor() const {return stream.get_executor();}

  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);


  Stream & stream;

  http::verb method;
  core::string_view target;
  http::fields &header;

  struct state_t
  {
    source &src;

    state_t(source &src) : src(src) {}

    optional<std::size_t> sz = src.size();
    variant2::variant<variant2::monostate,
                      http::request<http::empty_body>,
                      http::request<detail::fixed_source_body>,
                      http::request<detail::source_body>> freq;

    char prebuffer[BOOST_REQUESTS_CHUNK_SIZE];
    std::pair<std::size_t, bool> init;

  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(boost::system::error_code, std::size_t)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  async_write_request_op(allocator_type alloc,
                         Stream & stream,
                         http::verb method,
                         core::string_view target,
                         http::fields &header,
                         source &src)
    : stream(stream), method(method), target(target), header(header)
    , state(allocate_unique<state_t>(alloc, src))
  {}

  template<typename Self>
  void operator()(Self && self,
                  system::error_code ec = {}, std::size_t n = 0u)
  {
    auto st = state.get();
    BOOST_ASIO_CORO_REENTER(this)
    {
      {
        const auto itr = header.find(beast::http::field::content_type);
        if (itr == header.end())
        {
          auto def = st->src.default_content_type();
          if (!def.empty())
            header.set(beast::http::field::content_type, def);
        }
      }
      st->src.reset();

      if (st->sz)
      {
        if (*st->sz == 0)
        {
          st->freq.template emplace<1>(method, target, 11, http::empty_body::value_type(), std::move(header)).prepare_payload();
          BOOST_REQUESTS_YIELD beast::http::async_write(stream, variant2::get<1>(st->freq), std::move(self));
          header = std::move(variant2::get<1>(st->freq).base());
        }
        else
        {
          st->freq.template emplace<2>(method, target, 11, st->src, std::move(header)).prepare_payload();
          BOOST_REQUESTS_YIELD beast::http::async_write(stream, variant2::get<2>(st->freq), std::move(self));
          header = std::move(variant2::get<2>(st->freq).base());
        }

      }
      else
      {
        st->init = st->src.read_some(st->prebuffer, sizeof(st->prebuffer), ec);
        if (ec)
        {
          n = 0u;
          break;
        }

        st->freq.template emplace<3>(method, target, 11,
                     detail::source_body::value_type{st->src,
                                                     asio::buffer(st->prebuffer, st->init.first),
                                                     st->init.second}, std::move(header));

        if (!st->init.second)
          variant2::get<3>(st->freq).set(beast::http::field::content_length, std::to_string(st->init.first));
        else
          variant2::get<3>(st->freq).prepare_payload();
        BOOST_REQUESTS_YIELD beast::http::async_write(stream, variant2::get<3>(st->freq), std::move(self));
        header = std::move(variant2::get<3>(st->freq).base());
      }
    }
    if (is_complete())
    {
      state.reset();
      self.complete(ec, n);
    }
  }
};


}

template<typename Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code, std::size_t))
async_write_request(
    Stream & stream,
    http::verb method,
    core::string_view target,
    http::fields &header,
    source &src,
    CompletionToken && token)
{
  return asio::async_initiate<CompletionToken, void(boost::system::error_code, std::size_t)>(
      [](asio::any_completion_handler<void(boost::system::error_code, std::size_t)> handler,
         Stream * stream,
         http::verb method,
         core::string_view target,
         http::fields *header,
         source *src)
      {
        asio::async_compose<asio::any_completion_handler<void(boost::system::error_code, std::size_t)>,
                            void(boost::system::error_code, std::size_t)>(
            detail::async_write_request_op<Stream>{
                asio::get_associated_allocator(handler),
                *stream, method, target, *header, *src},
            handler, stream->get_executor());
      }, token, &stream, method, target, &header, &src);
}


}
}

#endif //BOOST_REQUESTS_IMPL_SOURCE_HPP
