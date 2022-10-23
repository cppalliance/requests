//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/error.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>

#include <boost/beast/http/rfc7230.hpp>
#include <boost/io/quoted.hpp>

#include "doctest.h"

#include <iostream>

TEST_CASE("FOO")
{
  using boost::beast::http::ext_list;
  using boost::beast::http::param_list;
  using boost::beast::http::token_list;

  std::cout << boost::io::quoted("FOOBAR \n \\ \? \n \" asd") << std::endl;

  boost::core::string_view value = R"(lnk, <https://one.example.com>; rel="preconnect", <https://two.example.com>; rel="preconnect", <https://three.example.com>; rel="preconnect")";

  for(auto const& ext : ext_list{value})
  {
    std::cout << ext.first << "\n";
    for(auto const& param : ext.second)
    {
      std::cout << ";" << param.first;
      if(! param.second.empty())
        std::cout << "=" << param.second;
      std::cout << "\n";
    }
  }


  std::cout << "----- param_list -----\n";
  for(auto const& ext : param_list{value})
  {
    std::cout << ext.first << " : " << ext.second <<  "\n";
  }


  using boost::beast::http::ext_list;
  std::cout << "----- token_list -----\n";
  for(auto const& ext : token_list{value})
  {
    std::cout << ext <<  "\n";
  }
}