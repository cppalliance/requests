//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef STRING_MAKER_HPP
#define STRING_MAKER_HPP


#include "doctest.h"
#include <boost/core/detail/string_view.hpp>
#include <boost/requests/fields/set_cookie.hpp>
#include <boost/system/result.hpp>

namespace doctest
{

template<>
struct StringMaker<boost::core::string_view>
{
  static String convert(boost::core::string_view sv)
  {
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::system::error_code>
{
  static String convert(boost::system::error_code ec)
  {
    return toString(ec.message());
  }
};

template<typename T>
struct StringMaker<boost::system::result<T>>
{
  static String convert(const boost::system::result<T> & res)
  {
    if(res.has_value())
      return toString(*res);
    else
      return toString(res.error());
  }
};


template<>
struct StringMaker<boost::requests::set_cookie::extensions_type>
{
  static String convert(const boost::requests::set_cookie::extensions_type & res)
  {
    std::string val;
    for (auto && attr : res)
    {
      val += attr;
      val += "; ";
    }
    return String(val.data(), val.size());
  }
};


}

#endif