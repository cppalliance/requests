//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/url.hpp>
#include <boost/requests/connection.hpp>
#include <boost/requests/json.hpp>
#include <boost/asio/use_future.hpp>

#include <iostream>

using namespace boost ;


int main(int argc, char * argv[])
{
  urls::url_view url{"unix:///var/run/docker.sock"};

  using my_conn = requests::connection;
  asio::io_context ctx;
  my_conn sock{ctx};

  sock.connect(asio::local::stream_protocol::endpoint{url.path()});
  sock.set_host("localhost");

  auto res = requests::json::get<json::array>(sock, urls::url_view{"/containers/json"});

  // list all containers
  std::cout << "Response: " << res.headers << std::endl;
  std::cout << "Amount of containers: " << res.value.size() << std::endl;

  for (auto val : res.value)
    std::cout << "Container[" << val.at("Id") << "]: " << json::serialize(val.at("Names")) << std::endl;


  return 0;
}