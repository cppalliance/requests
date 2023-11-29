//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_QUOTED_STRING_HPP
#define BOOST_REQUESTS_RFC_QUOTED_STRING_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>

namespace boost {
namespace requests {
namespace rfc {

/** A quoted string according to rfc 2616

    @par Example
    @code
    auto rv = parse( R"("preconnect")", quoted_string );
    @endcode

    @par BNF
    @code
      quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
      qdtext         = <any TEXT except <">>

      quoted-pair    = "\" CHAR
    @endcode

    @par Specification
    @li <a href="https://www.rfc-editor.org/rfc/rfc7230#section-3.2.6"
        >3.2.6 Basic Rules</a>
    */
#ifdef BOOST_REQUESTS_DOCS
constexpr __implementation_defined__ quoted_string;
#else
struct quoted_string_t
{
  using value_type = core::string_view;

  BOOST_REQUESTS_DECL auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>;

};

constexpr quoted_string_t quoted_string{};
#endif

BOOST_REQUESTS_DECL bool is_quoted_string(core::string_view sv);
BOOST_REQUESTS_DECL bool unquoted_size(core::string_view sv);

template<typename Allocator = std::allocator<char>>
auto unquote_string(
    core::string_view sv, Allocator && alloc = {})
        -> std::basic_string<char, std::char_traits<char>, Allocator>
{
  std::basic_string<char, std::char_traits<char>, Allocator> res{std::forward<Allocator>(alloc)};
  res.reserve(unquoted_size(sv));

  if (sv.empty());
  else if (sv.front() != '"' || sv.back() != '"')
    res.assign(sv.begin(), sv.end());
  else
  {
    auto ss = sv.substr(1u, sv.size() - 2u);
    for (auto itr = ss.begin(); itr != ss.end(); itr++)
    {
      if (*itr == '\\')
        itr++;
      res.push_back(*itr);
    }
  }
  return res;
}


}
}
}

#endif // BOOST_REQUESTS_RFC_QUOTED_STRING_HPP
