// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/redirect.hpp>
#include <boost/url/parse.hpp>

#include "doctest.h"

TEST_SUITE_BEGIN("redirect");


TEST_CASE("none")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(!should_redirect(redirect_mode::none, parse_uri("http://boost.org").value(), parse_uri("http://boost.org/index.html").value()));
    CHECK(!should_redirect(redirect_mode::none, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
}


TEST_CASE("endpoint")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("http://boost.org/index.html").value()));
    CHECK(!should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("https://boost.org").value()));
    CHECK(!should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("http://www.boost.org").value()));
    CHECK(!should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:433").value()));
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("https://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("ws://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri("foo://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::endpoint, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
}

TEST_CASE("domain")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org/index.html").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org").value()));
    CHECK(!should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("http://www.boost.org").value()));
    CHECK(!should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("http://fakeboost.org").value()));
    CHECK(!should_redirect(redirect_mode::domain, parse_uri("http://www.boost.org").value(), parse_uri("http://boost.org").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:433").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("ws://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri("foo://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::domain, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
    }

TEST_CASE("subdomain")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org/index.html").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("http://www.boost.org").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("https://www.boost.org").value()));
    CHECK(!should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("http://fakeboost.org").value()));
    CHECK(!should_redirect(redirect_mode::subdomain, parse_uri("http://www.boost.org").value(), parse_uri("http://boost.org").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:433").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("ws://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri("foo://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::subdomain, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
}

TEST_CASE("private_domain")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org/index.html").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("http://www.boost.org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("https://www.boost.org").value()));
    CHECK(!should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("http://fakeboost.org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://www.boost.org").value(), parse_uri("http://boost.org").value()));
    CHECK(!should_redirect(redirect_mode::private_domain, parse_uri("http://www.boost.org").value(), parse_uri("http://org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:433").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("https://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("http://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("ws://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri("foo://boost.org:80").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://doc.boost.org").value(), parse_uri("http://docs.boost.org").value()));
    CHECK(!should_redirect(redirect_mode::private_domain, parse_uri("http://doc.boost.org").value(), parse_uri("http://fakeboost.org").value()));
    CHECK(!should_redirect(redirect_mode::private_domain, parse_uri("http://doc.boost.org").value(), parse_uri("http://ost.org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://api.boost.org").value(), parse_uri("http://pi.boost.org").value()));
    CHECK(should_redirect(redirect_mode::private_domain, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
}


TEST_CASE("any")
{
    using namespace boost::requests;
    using namespace boost::urls;
    CHECK(should_redirect(redirect_mode::any, parse_uri("http://boost.org").value(), parse_uri("https://take-my-money.com").value()));
    CHECK(should_redirect(redirect_mode::any, parse_uri("http://boost.org").value(), parse_uri_reference("/take-my-money.com").value()));
}


TEST_SUITE_END();