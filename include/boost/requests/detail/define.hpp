//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control/if.hpp>

#define CHECK_ERROR_1(Expr) Expr(ec);                     \
  if (!ec.has_location())                                 \
  {                                                       \
      constexpr auto loc = BOOST_CURRENT_LOCATION;        \
      ec = system::error_code(ec, &loc);                  \
  }                                                       \
  if (ec)                                                 \
        return

#define CHECK_ERROR_N(Expr, ...) Expr(__VA_ARGS__, ec);     \
    if (!ec.has_location())                                 \
    {                                                       \
        constexpr auto loc = BOOST_CURRENT_LOCATION;        \
        ec = system::error_code(ec, &loc);                  \
    }                                                       \
    if (ec)                                                 \
        return

#define CHECK_ERROR(...)  \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), CHECK_ERROR_1, CHECK_ERROR_N)(__VA_ARGS__)

#define STATE(Name) if (false) Name:

#define AINIT template<typename Self> void operator()(Self && self)

#define ASTATE_1(Name)  \
  struct Name##_tag {}; \
  template<typename Self> void operator()(Self && self, Name##_tag)


#define ASTATE_N(Name, ...) \
  struct Name##_tag {}; \
  template<typename Self> void operator()(Self && self, Name##_tag, __VA_ARGS__)

#define ASTATE(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), ASTATE_1, ASTATE_N)(__VA_ARGS__)

#define ANEXT_1(State) \
  boost::asio::prepend(std::move(self), State##_tag{})


#define ANEXT_N(State, ...) \
  boost::asio::prepend(boost::asio::append(std::move(self), __VA_ARGS__), State##_tag{})


#define ANEXT(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), ANEXT_1, ANEXT_N)(__VA_ARGS__)

#define AGOTO_1(State)  \
  return (*this)(std::move(self), State##_tag{})


#define AGOTO_N(State, ...) \
  return (*this)(std::move(self), State##_tag{}, __VA_ARGS__)

#define AGOTO(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), AGOTO_1, AGOTO_N)(__VA_ARGS__)

#define ACOMPLETE(...) \
  return self.complete(__VA_ARGS__)
