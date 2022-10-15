// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_REDIRECT_HPP
#define BOOST_REQUESTS_REDIRECT_HPP

#include <boost/requests/public_suffix.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/url/scheme.hpp>
#include <boost/url/url_view.hpp>

namespace boost
{
namespace requests
{

enum redirect_mode
{
    /// Follow no redirect  at all.
    none,
    /// Follow redirect on the same endpoint, i.e. different target
    endpoint,
    /// Follow redirects on the same domain, e.g. http -> https
    domain,
    /// Follow redirects to subdomains, e.g. boost.org -> www.boost.org but not vice versa
    subdomain,
    /// Follow redirects withing a non-public suffix, e.g.
    /// www.boost.org -> boost.org or api.boost.org, but not get-hacked.org.
    private_domain,
    /// Follow any redirect
    any
};

BOOST_REQUESTS_DECL bool should_redirect(
        redirect_mode mode,
        urls::url_view current,
        urls::url_view target,
        const public_suffix_list & pse = default_public_suffix_list());

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/redirect.ipp>
#endif

#endif //BOOST_REQUESTS_REDIRECT_HPP
