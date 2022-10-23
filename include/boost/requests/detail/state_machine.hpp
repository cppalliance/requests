//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_STATE_MACHINE_HPP
#define BOOST_REQUESTS_STATE_MACHINE_HPP

#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/variadic/size.hpp>

#define BOOST_REQUESTS_CHECK_ERROR_1(Expr) Expr(ec);                     \
  if (!ec.has_location())                                 \
  {                                                       \
      constexpr auto loc = BOOST_CURRENT_LOCATION;        \
      ec = system::error_code(ec, &loc);                  \
  }                                                       \
  if (ec)                                                 \
        return

#define BOOST_REQUESTS_CHECK_ERROR_N(Expr, ...) Expr(__VA_ARGS__, ec);     \
    if (!ec.has_location())                                 \
    {                                                       \
        constexpr auto loc = BOOST_CURRENT_LOCATION;        \
        ec = system::error_code(ec, &loc);                  \
    }                                                       \
    if (ec)                                                 \
        return

#define BOOST_REQUESTS_CHECK_ERROR(...)  \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BOOST_REQUESTS_CHECK_ERROR_1, BOOST_REQUESTS_CHECK_ERROR_N)(__VA_ARGS__)

#define BOOST_REQUESTS_STATE(Name) if (false) Name:
/*
#define BOOST_REQUESTS_AINIT template<typename Self> void operator()(Self && self)

#define BOOST_REQUESTS_ASTATE_1(Name)  \
  struct Name##_tag {}; \
  template<typename Self> void operator()(Self && self, Name##_tag)


#define BOOST_REQUESTS_ASTATE_N(Name, ...) \
  struct Name##_tag {}; \
  template<typename Self> void operator()(Self && self, Name##_tag, __VA_ARGS__)

#define BOOST_REQUESTS_ASTATE(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BOOST_REQUESTS_ASTATE_1, BOOST_REQUESTS_ASTATE_N)(__VA_ARGS__)

#define BOOST_REQUESTS_ANEXT_1(State) \
  boost::asio::prepend(std::move(self), State##_tag{})


#define BOOST_REQUESTS_ANEXT_N(State, ...) \
  boost::asio::prepend(boost::asio::append(std::move(self), __VA_ARGS__), State##_tag{})


#define BOOST_REQUESTS_ANEXT(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BOOST_REQUESTS_ANEXT_1, BOOST_REQUESTS_ANEXT_N)(__VA_ARGS__)

#define BOOST_REQUESTS_AGOTO_1(State)  \
  return (*this)(std::move(self), State##_tag{})


#define BOOST_REQUESTS_AGOTO_N(State, ...) \
  return (*this)(std::move(self), State##_tag{}, __VA_ARGS__)

#define BOOST_REQUESTS_AGOTO(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BOOST_REQUESTS_AGOTO_1, BOOST_REQUESTS_AGOTO_N)(__VA_ARGS__)

#define BOOST_REQUESTS_ACOMPLETE(...) \
  return self.complete(__VA_ARGS__)*/


#endif // BOOST_REQUESTS_STATE_MACHINE_HPP
