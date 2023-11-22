// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/cookie_jar.hpp>
#include <boost/requests/public_suffix.hpp>
#include <thread>
#include <boost/test/unit_test.hpp>

namespace rcp = boost::requests;


BOOST_AUTO_TEST_SUITE(cookie_jar);

BOOST_AUTO_TEST_CASE(public_list)
{
    auto l = rcp::default_public_suffix_list();

    BOOST_CHECK(rcp::is_public_suffix("com", l));
    BOOST_CHECK(rcp::is_public_suffix("org", l));
    BOOST_CHECK(!rcp::is_public_suffix("boost.org", l));
    BOOST_CHECK(!rcp::is_public_suffix("x.yz.bd", l));
    BOOST_CHECK(!rcp::is_public_suffix("city.kobe.jp", l));
}


BOOST_AUTO_TEST_CASE(domain_match)
{
    BOOST_CHECK(rcp::domain_match("foo.com", "com"));
    BOOST_CHECK(rcp::domain_match("com", "com"));
    BOOST_CHECK(!rcp::domain_match("foocom", "com"));
}

BOOST_AUTO_TEST_CASE(cookie_jar)
{
    rcp::cookie_jar j;
    j.set(rcp::parse_set_cookie_field("userid=sup3r4n0m-us3r-1d3nt1f13r").value(), "boost.org");
    j.set(rcp::parse_set_cookie_field("lib=requests; Max-Age=10").value(), "boost.org");
    j.set(rcp::parse_set_cookie_field("doc=foobar; Max-Age=0").value(), "boost.org");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    BOOST_CHECK(j.get("boost.org") == "lib=requests; userid=sup3r4n0m-us3r-1d3nt1f13r");

}





BOOST_AUTO_TEST_SUITE_END();

