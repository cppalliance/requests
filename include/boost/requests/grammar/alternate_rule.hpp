//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_GRAMMAR_ALTERNATE_RULE_HPP
#define BOOST_REQUESTS_GRAMMAR_ALTERNATE_RULE_HPP

#include <boost/system/result.hpp>
#include <boost/url/grammar/parse.hpp>
#include <tuple>

namespace boost {
namespace requests {
namespace grammar {

/** Match one of a set of rules, where all rules yield the same type.

    Each specified rule is tried in sequence.
    When the first match occurs, the result
    is stored and returned in the variant. If
    no match occurs, an error is returned.

    @par Value Type
    @code
    using value_type = variant< typename Rules::value_type... >;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    // request-target = origin-form
    //                / absolute-form
    //                / authority-form
    //                / asterisk-form

    result< variant< url_view, url_view, authority_view, string_view > > rv = grammar::parse(
        "/index.html?width=full",
        alternate_rule(
            origin_form_rule,
            absolute_uri_rule,
            authority_rule,
            delim_rule('*') ) );
    @endcode

    @par BNF
    @code
    variant     = rule1 / rule2 / rule3...
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc5234#section-3.2"
        >3.2.  Alternatives (rfc5234)</a>
    @li <a href="https://datatracker.ietf.org/doc/html/rfc7230#section-5.3"
        >5.3.  Request Target (rfc7230)</a>

    @see
        @ref absolute_uri_rule,
        @ref authority_rule,
        @ref delim_rule,
        @ref parse,
        @ref origin_form_rule,
        @ref url_view,
        @ref variant.
*/
#ifdef BOOST_URL_DOCS
template<class... Rules>
constexpr
    __implementation_defined__
    alternate_rule( Rules... rn ) noexcept;
#else

namespace detail
{


// must come first
template<
    class R0,
    class... Rn,
    std::size_t I>
auto
    parse_alternate(
    char const*&,
    char const*,
    std::tuple<
        R0, Rn...> const&,
    std::integral_constant<
        std::size_t, I> const&,
    std::false_type const&) ->
    system::result<typename R0::value_type>
{
  // no match
  BOOST_REQUESTS_RETURN_EC(
      urls::grammar::error::mismatch);
}

template<
    class R0,
    class... Rn,
    std::size_t I>
auto
parse_alternate(
    char const*& it,
    char const* const end,
    std::tuple<R0, Rn...> const& rn,
    std::integral_constant<
        std::size_t, I> const&,
    std::true_type const&) ->
    system::result<typename R0::value_type>
{
  auto const it0 = it;
  auto rv = parse(
      it, end, std::get<I>(rn));
  if( rv )
    return typename R0::value_type(*rv);
  it = it0;
  return parse_alternate(
      it, end, rn,
      std::integral_constant<
          std::size_t, I+1>{},
      std::integral_constant<bool,
                             ((I + 1) < (1 +
                                         sizeof...(Rn)))>{});
}

}

template<
    class R0, class... Rn>
class alternate_rule_t
{
public:
  using value_type = typename R0::value_type;

  auto
  parse(
      char const*& it,
      char const* end) const ->
      system::result<value_type>
  {
    return detail::parse_alternate(
        it, end, rn_,
        std::integral_constant<
            std::size_t, 0>{},
        std::true_type{});
  }

  template<
      class R0_,
      class... Rn_>
  friend
      constexpr
      auto
      alternate_rule(
          R0_ const& r0,
          Rn_ const&... rn) noexcept ->
      alternate_rule_t<R0_, Rn_...>;

private:
  constexpr
      alternate_rule_t(
          R0 const& r0,
          Rn const&... rn) noexcept
      : rn_(r0, rn...)
  {
  }

  std::tuple<R0, Rn...> rn_;
};

template<
    class R0_,
    class... Rn_>
constexpr
    auto
    alternate_rule(
        R0_ const& r0,
        Rn_ const&... rn) noexcept ->
    alternate_rule_t<R0_, Rn_...>
{
  return { r0, rn... };

}

#endif

} // grammar
} // requests
} // boost

#endif // BOOST_REQUESTS_GRAMMAR_ALTERNATE_RULE_HPP
