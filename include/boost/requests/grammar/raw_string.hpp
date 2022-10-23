//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_GRAMAR_RAW_STRING_HPP
#define BOOST_REQUESTS_GRAMAR_RAW_STRING_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>

namespace boost {
namespace requests {
namespace grammar {


/// Wrapper for any rule to make it a string_view.

template<class InnerRule>
struct raw_string_t
{
  using value_type = core::string_view;

  auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>
  {
    auto it0 = it;
    auto res = inner_rule_.parse(it, end);
    if (res.has_error())
      return res.error();
    else
      return core::string_view(it0, end);
  }

private:
  InnerRule inner_rule_;

  template<class InnerRule_>
  friend
  constexpr
      auto
      raw_string(InnerRule_ &&) noexcept ->
      raw_string_t<std::decay_t<InnerRule_>>;


  template<class InnerRule_>
  constexpr raw_string_t(InnerRule_ && inner_rule)
      : inner_rule_(std::forward<InnerRule_>(inner_rule))
  {
  }

};

template<class InnerRule>
constexpr
    auto
    raw_string(InnerRule && inner_rule) noexcept ->
    raw_string_t<std::decay_t<InnerRule>>
{
  return raw_string_t<std::decay_t<InnerRule>>{std::forward<InnerRule>(inner_rule)};
}

}
}
}

#endif // BOOST_REQUESTS_GRAMAR_RAW_STRING_HPP
