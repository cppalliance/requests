//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_future.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/service.hpp>
#include <iostream>

using namespace boost;

int main(int argc, char * argv[])
{
  asio::io_context ctx;

  requests::session sess{ctx};
  auto stream = request_stream(sess, requests::http::verb::get,
                           urls::url_view{argv[1]},
                           requests::empty(),
                           requests::http::fields()).first;

  std::string buf;


  char chunk[32];
  std::function<void(system::error_code, std::size_t)> c;

  c = [&](system::error_code ec, std::size_t n)
      {
        std::string str(chunk, n);
        printf("Chunk [%ld]: '%s'\n", n, str.c_str());
        if (stream.is_open())
          stream.async_read_some(asio::buffer(chunk), c);
      };

  stream.async_read_some(asio::buffer(chunk), c);

  printf("RUN %ld\n", ctx.run());
  return 0;
}