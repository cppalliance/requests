//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_KEEP_ALIVE_IPP
#define BOOST_REQUESTS_IMPL_KEEP_ALIVE_IPP

#include <boost/requests/fields/keep_alive.hpp>

#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/variant_rule.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/alnum_chars.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/range_rule.hpp>

namespace boost
{
namespace requests
{
/*
 *
   Keep-Alive           = "Keep-Alive" ":" 1#keep-alive-info
   keep-alive-info      =   "timeout" "=" delta-seconds
                          / keep-alive-extension
   keep-alive-extension = token [ "=" ( token / quoted-string ) ]
 */

system::result<keep_alive>  parse_keep_alive_field(
            core::string_view value,
            std::chrono::system_clock::time_point now)
{
  namespace ug = urls::grammar;
  constexpr auto kvp = ug::tuple_rule(
              ug::token_rule(ug::alpha_chars),
              ug::squelch(ug::literal_rule("=")),
              ug::token_rule(ug::alnum_chars)
          );

  constexpr ug::lut_chars space = " ";

  auto res = ug::parse(value,
                     ug::range_rule(
                         kvp,
                         ug::tuple_rule(
                           ug::squelch(
                             ug::tuple_rule(
                               ug::literal_rule(","),
                               ug::token_rule(space))),
                              kvp)));
  if (res.has_error())
    return res.error();

  keep_alive ka;

  for (const auto & kv : *res)
  {
    const auto & k = get<0>(kv);
    const auto & v = get<1>(kv);

    using sc = std::chrono::system_clock;
    if (ug::ci_is_equal(k, "timeout"))
    {
      const auto e = parse(v, ug::token_rule(ug::digit_chars));
      if (e.has_error())
        return e.error();
      ka.timeout = sc::time_point(std::chrono::seconds(std::stoull(v)));
    }
    else if (ug::ci_is_equal(k, "max"))
    {
      const auto e = parse(v, ug::token_rule(ug::digit_chars));
      if (e.has_error())
        return e.error();
      ka.max = std::stoull(v);
    }
  }

  return ka;
}


}
}

#endif //BOOST_REQUESTS_IMPL_KEEP_ALIVE_IPP
