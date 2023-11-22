//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/json.hpp>

namespace boost
{
namespace requests
{

auto as_json(const response & res,
                    json::storage_ptr ptr,
                    system::error_code & ec) -> json::value
{
  boost::json::parser ps;
  ps.write(res.string_view(), ec);
  if (ec)
    return nullptr;
  else
    return ps.release();
}

auto as_json(const response & res, json::storage_ptr ptr) -> json::value
{
  boost::system::error_code ec;
  auto rs = as_json(res, ptr, ec);
  if (ec)
    urls::detail::throw_system_error(ec);

  return rs;
}

auto as_json(const response & res, system::error_code & ec) -> json::value
{
  return as_json(res, json::storage_ptr(), ec);
}

namespace json
{


json::value read_json(stream & str, json::storage_ptr ptr)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  return sp.release();
}

json::value read_json(stream & str, json::storage_ptr ptr, system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !ec && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer), ec);
    if (ec)
      break;
    sp.write_some(buffer, n, ec);
  }
  if (!ec)
    sp.finish(ec);
  if (ec)
    return json::value();

  return sp.release();
}

namespace detail
{

struct async_read_json_op : asio::coroutine
{
  constexpr static const char * op_name = "async_read_json_op";
  using executor_type = typename stream::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  stream &str;

  struct state_t
  {
    state_t(json::storage_ptr ptr) : sp(std::move(ptr)) {}
    ::boost::json::stream_parser sp;
    char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  };

  using allocator_type = asio::any_completion_handler_allocator<void, void(error_code, json::value)>;
  std::unique_ptr<state_t, alloc_deleter<state_t, allocator_type>> state;

  async_read_json_op(allocator_type alloc, stream * str, json::storage_ptr ptr)
      : str(*str), state{allocate_unique<state_t>(alloc, std::move(ptr))} {}

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      while (!state->sp.done() && !str.done() && !ec)
      {
        BOOST_REQUESTS_YIELD str.async_read_some(asio::buffer(state->buffer), std::move(self));
        if (ec)
          return self.complete(ec, nullptr);
        state->sp.write_some(state->buffer, n, ec);
      }
      if (!ec)
        state->sp.finish(ec);
      if (ec)
        return self.complete(ec, nullptr);

      auto r = state->sp.release();
      state.reset();
      return self.complete(ec, std::move(r));
    }
  }
};

void async_read_json_impl(
    asio::any_completion_handler<void(error_code, json::value)> handler,
    stream * str, json::storage_ptr ptr)
{
  return asio::async_compose<asio::any_completion_handler<void(error_code, json::value)>,
                             void(error_code, json::value)>(
      async_read_json_op{
          asio::get_associated_allocator(handler),
          str, std::move(ptr)},
      handler,
      str->get_executor());
}

}

}

}
}