//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FIELDS_LINK_HPP
#define BOOST_REQUESTS_FIELDS_LINK_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/url/url_view.hpp>

namespace boost
{
namespace requests
{

struct link
{
  urls::url_view url;

  struct field
  {
    core::string_view key;
    core::string_view value;
  };


  struct extensions_only
  {
    BOOST_REQUESTS_DECL bool operator()(const core::string_view & ra) const;
    bool operator()(const field & attr) const
    {
      return (*this)(attr.key);
    }
  };


  using attributes_type = urls::grammar::range<field>;
  attributes_type attributes;

  BOOST_REQUESTS_DECL system::result<urls::grammar::range<core::string_view>> rel() const;
  BOOST_REQUESTS_DECL system::result<urls::url_view> anchor() const;
  BOOST_REQUESTS_DECL system::result<urls::grammar::range<core::string_view>> rev() const;

  template<typename Allocator = std::allocator<char>>
  auto type(Allocator && alloc = {}) const -> std::basic_string<char, std::char_traits<char>, Allocator>
  {
      const auto begin = attributes.begin();
      const auto end = attributes.end();

      const auto itr = std::find_if(begin,end,
                                      [&](const field & attr){
                                        return urls::grammar::ci_is_equal(attr.key, "type");
                                      });

        if (itr == end)
          return urls::grammar::error::out_of_range;

        return unquote_string((*itr).value, std::move(alloc));

  }
  using extensions_type = range_detail::filtered_range<extensions_only, const attributes_type>;
  extensions_type extensions() const
  {
    return attributes | adaptors::filtered(extensions_only{});
  }


};

BOOST_REQUESTS_DECL system::result<urls::grammar::range<link>> parse_link_field(core::string_view value );

}
}


#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/fields/impl/link.ipp>
#endif


#endif // BOOST_REQUESTS_FIELDS_LINK_HPP
