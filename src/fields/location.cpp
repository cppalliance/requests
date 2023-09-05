//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/fields/location.hpp>

namespace boost
{
namespace requests
{

system::result<urls::url_view> interpret_location(
    core::string_view current_target,
    core::string_view location)
try
{
  static thread_local urls::static_url<BOOST_REQUESTS_MAX_URL_LENGTH> buffer;
  auto nw = urls::parse_uri_reference(location);
  if (nw.has_error())
    return nw.error();

  if (nw->is_path_absolute())
  {
    if (!nw->has_fragment())
    {
      auto ct = urls::parse_uri_reference(current_target);
      if (ct.has_error())
        return ct.error();

      if (ct->has_fragment())
      {
        buffer = *nw;
        buffer.set_encoded_fragment(ct->encoded_fragment());
        return buffer;
      }
    }
    return nw;
  }

  auto ct = urls::parse_uri_reference(current_target);
  if (ct.has_error())
    return ct.error();

  buffer = *ct;

  auto segs = buffer.encoded_segments();
  for (auto ns : nw->encoded_segments())
    segs.push_back(ns);

  if (nw->has_fragment())
    buffer.set_encoded_fragment(nw->encoded_fragment());
  buffer.normalize();
  return buffer;
}
catch(system_error & se)
{
  return se.code();
}

}
}
