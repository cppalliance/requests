//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_IMPL_LINK_IPP
#define BOOST_REQUESTS_RFC_IMPL_LINK_IPP

#include <boost/requests/grammar/attr_char.hpp>
#include <boost/requests/grammar/fixed_token_rule.hpp>
#include <boost/requests/grammar/mime_char.hpp>
#include <boost/requests/grammar/ptokenchar.hpp>
#include <boost/requests/grammar/raw_string.hpp>
#include <boost/requests/rfc/link.hpp>
#include <boost/requests/rfc/quoted_string.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/rfc/uri_reference_rule.hpp>

namespace boost {
namespace requests {
namespace rfc {


struct link_attribute_rule_t
{
  using value_type = typename requests::link::field;
  auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>
  {

    namespace ug = boost::urls::grammar;

    constexpr auto sp = ug::range_rule(ug::delim_rule(' '));


    constexpr auto wurl = ug::tuple_rule(ug::squelch(ug::delim_rule('<')),
                                         urls::uri_reference_rule,
                                         ug::squelch(ug::delim_rule('>')));

    constexpr auto rl = ug::tuple_rule(
        ug::squelch(
            ug::tuple_rule(
                ug::delim_rule(';'),
                sp
                )
                ),
        ug::token_rule(grammar::attr_char),
        ug::optional_rule(
            ug::tuple_rule(
                ug::squelch(ug::delim_rule('=')),
                grammar::raw_string(
                    ug::variant_rule(
                        ug::token_rule(grammar::attr_char),
                        quoted_string,
                        wurl)
                        )
                    )
                )
    );

    auto res = ug::parse(it, end, rl);
    if (res.has_error())
      return res.error();

    const auto & val = res.value();

    return value_type{std::get<0>(val), std::get<1>(val).value_or("")};
  }
};

constexpr static link_attribute_rule_t link_attribute_rule{};

auto
link_value_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;
  using namespace detail;

  constexpr auto wurl = ug::tuple_rule(ug::squelch(ug::delim_rule('<')),
                                       urls::uri_reference_rule,
                                       ug::squelch(ug::delim_rule('>')));


  constexpr auto lnk = ug::tuple_rule(
      wurl,
      ug::range_rule(link_attribute_rule));

  auto r =  ug::parse(it, end, lnk);

  if (r.has_error())
    return r.error();

  return value_type{std::get<0>(r.value()), std::get<1>(r.value())};
}


}
}
}

#endif // BOOST_REQUESTS_RFC_IMPL_LINK_IPP
