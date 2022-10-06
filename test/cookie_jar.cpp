// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/cookies/public_suffix.hpp>
#include <boost/requests/cookies/jar.hpp>
#include <thread>

namespace rcp = boost::requests::cookies;

#include "doctest.h"

TEST_SUITE_BEGIN("cookie-jar");

TEST_CASE("public-list")
{
    auto l = rcp::default_public_suffix_list();

    CHECK(rcp::is_public_suffix("com", l));
    CHECK(rcp::is_public_suffix("org", l));
    CHECK(!rcp::is_public_suffix("boost.org", l));
    CHECK(rcp::is_public_suffix("xyz.bd", l));
    CHECK(!rcp::is_public_suffix("x.yz.bd", l));
    CHECK(!rcp::is_public_suffix("city.kobe.jp", l));
    CHECK(rcp::is_public_suffix("shop.kobe.jp", l));
}


TEST_CASE("domain-match")
{
    CHECK(rcp::domain_match("foo.com", "com"));
    CHECK(rcp::domain_match("com", "com"));
    CHECK(!rcp::domain_match("foocom", "com"));
}

TEST_CASE("cookie-jar")
{
    rcp::jar j;
    j.set(rcp::parse_set_cookie_field("userid=sup3r4n0m-us3r-1d3nt1f13r").value(), "boost.org");
    j.set(rcp::parse_set_cookie_field("lib=requests; Max-Age=10").value(), "boost.org");
    j.set(rcp::parse_set_cookie_field("doc=foobar; Max-Age=0").value(), "boost.org");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    CHECK(j.get("boost.org") == "lib=requests; userid=sup3r4n0m-us3r-1d3nt1f13r");

}





TEST_SUITE_END();

