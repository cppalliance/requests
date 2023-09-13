//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/cookie_jar.hpp>

namespace boost {
namespace requests {

// https://www.rfc-editor.org/rfc/rfc6265#section-5.3
// the string needs to be normalized to lower-case!
bool domain_match(core::string_view full, core::string_view pattern)
{
  if (full.ends_with(pattern))
  {
    if (full.size() == pattern.size())
      return true;
    else
      return full[full.size() - pattern.size() - 1] == '.';
  }
  return false;
}

// the string needs to be normalized to lower-case!
bool path_match(core::string_view full, core::string_view pattern)
{
  if (full.starts_with(pattern))
  {
    if (full.size() == pattern.size())
      return true;
    else if (pattern.ends_with('/'))
      return true;
    else
      return full[pattern.size()] == '/';
  }
  return false;
}


bool cookie_jar::set(const set_cookie & set,
                     core::string_view request_host,
                     bool from_non_http_api,
                     urls::pct_string_view request_uri_path,
                     const public_suffix_list & public_suffixes)
{

  // https://www.rfc-editor.org/rfc/rfc6265#section-5.3

  // 2.   Create a new cookie with name cookie-name, value cookie-value.
  cookie sc{};
  sc.name = set.name;
  sc.value = set.value;
  const auto now = std::chrono::system_clock::now();

  // 3.   If the cookie-attribute-list contains an attribute with an attribute-name of "Max-Age":
  if (set.max_age != std::chrono::seconds::max())
  {
    sc.expiry_time = (sc.creation_time + set.max_age);
    sc.persistent_flag = false;
  }
  // Otherwise, if the cookie-attribute-list contains an attribute with an attribute-name of "Expires"
  else if (set.expires != std::chrono::system_clock::time_point::max())
  {
    sc.expiry_time = set.expires;
    sc.persistent_flag = true;
  }
  else // Set the cookie's persistent-flag to false.
  {
    sc.expiry_time = std::chrono::system_clock::time_point::max();
    sc.persistent_flag = false;
  }

  // 4. If the cookie-attribute-list contains an attribute with an attribute-name of "Domain":
  if (!set.domain.empty())
  {

    if (is_public_suffix(sc.domain, public_suffixes))
    {
      // ignore invalid cookie unless it's an exact match
      if (request_host != set.domain)
        return false;
    }
    else if (!domain_match(request_host, set.domain)) // ignore the cookie, trying to set the wrong hostname
      return false;

    sc.domain = set.domain;
    for (auto & c : sc.domain)
      c = urls::grammar::to_lower(c);
    sc.host_only_flag = false;
  }
  else
  {
    sc.host_only_flag = true;
    sc.domain.assign(request_host.begin(), request_host.end());
  }

  if (!set.path.empty())
    sc.path = set.path;
  else
  {
    if (request_uri_path.empty() || request_uri_path.size() == 1u)
      sc.path = "/";
    else
    {
      auto tmp = urls::parse_relative_ref(request_uri_path).value();
      sc.path.assign(tmp.encoded_path().begin(), tmp.encoded_path().end());
    }
    for (auto & c : sc.path)
      c = urls::grammar::to_lower(c);
  }
  for (auto & c : sc.path)
    c = urls::grammar::to_lower(c);

  sc.secure_only_flag = set.secure;
  sc.http_only_flag = set.http_only;
  if (from_non_http_api  && sc.http_only_flag)
    return false;

  //    11.  If the cookie store contains a cookie with the same name,
  //        domain, and path as the newly created cookie:
  auto itr = content.find(sc);
  if (itr != content.end())
  {
    if (itr->http_only_flag && from_non_http_api )
      return false;

    sc.creation_time = itr->creation_time;
    content.erase(itr);
  }

  if (sc.expiry_time > now)
    return content.insert(std::move(sc)).second;
  else
    return false;
}

void cookie_jar::drop_expired(const std::chrono::system_clock::time_point nw)
{
  for (auto itr = content.begin(); itr != content.end();)
  {
    if (itr->expiry_time < nw)
      itr = content.erase(itr);
    else
      itr ++;
  }
}

}
}

