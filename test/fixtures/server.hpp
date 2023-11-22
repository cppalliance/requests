//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SERVER_HPP
#define BOOST_REQUESTS_SERVER_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/process/v2/pid.hpp>
#include <boost/url/url.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>


struct test_server
{
  test_server(const test_server & ) = delete;

  using endpoint_type = typename boost::asio::generic::stream_protocol::endpoint;
  endpoint_type endpoint()
  {
    return ep_;
  }

  boost::core::string_view path() {return path_;}

  test_server();

  template<typename Socket>
  void run_session(Socket sock, boost::asio::yield_context ctx);

private:
  static std::size_t cnt_;
  template<typename Acceptor>
  void run_(Acceptor acc, boost::asio::yield_context ctx);
  endpoint_type ep_;
  boost::asio::thread_pool tp_{1u};
  std::string path_;
};

#endif // BOOST_REQUESTS_SERVER_HPP
