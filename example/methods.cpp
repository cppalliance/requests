//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/method.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/request_settings.hpp>
#include <boost/requests/service.hpp>
#include <boost/asio/use_future.hpp>
#include <iostream>
using namespace boost;

int main(int argc, char * argv[])
{
  auto r = requests::get("https://api.github.com/events");
  r = requests::post("https://httpbin.org/post", json::value{{"key", "value"}});
  std::cout << r.header << r.string_view() << std::endl;

  r = requests::put("https://httpbin.org/put", json::value{{"key","value"}});
  std::cout << r.header << r.string_view() << std::endl;

  r = requests::delete_("https://httpbin.org/delete");
  std::cout << r.header << r.string_view() << std::endl;

  r = requests::head("https://httpbin.org/get");
  std::cout << r.header << r.string_view() << std::endl;

  r = requests::options("https://httpbin.org/get");
  std::cout << r.header << r.string_view() << std::endl;
  return 0;
}