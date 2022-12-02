//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef STRING_MAKER_HPP
#define STRING_MAKER_HPP


#include "doctest.h"
#include <boost/core/demangle.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/requests/fields/set_cookie.hpp>
#include <boost/asio/multiple_exceptions.hpp>
#include <boost/system/result.hpp>
#include <boost/json.hpp>


namespace doctest
{


template<>
struct StringMaker<std::type_info >
{
  static String convert(const std::type_info  & t)
  {
    return String(boost::core::demangle(t.name()).c_str());
  }
};

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
    if (ec.has_location())
        return toString(ec.location().to_string() + ": " + ec.message());
    else
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

template<>
struct StringMaker<boost::json::value>
{
  static String convert(const boost::json::value & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::array>
{
  static String convert(const boost::json::array & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::object>
{
  static String convert(const boost::json::object & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::string>
{
  static String convert(const boost::json::string & sv)
  {
    return String(sv.data(), sv.size());
  }
};


template<>
struct StringMaker<std::exception_ptr>
{
  static String convert(std::exception_ptr e)
  {
    if (e)
        try
        {
          std::rethrow_exception(e);
        }
        catch(boost::asio::multiple_exceptions & me)
        {
           return convert(me.first_exception());

        }
        catch(std::exception & ex)
        {
          return ex.what();
        }
    else
        return "<null>";
  }
};

}

#endif