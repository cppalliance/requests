// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/detail/variadic.hpp>
#include <boost/requests/facade.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/core/demangle.hpp>
#include <boost/json.hpp>
#include <boost/type_index/ctti_type_index.hpp>

#include "doctest.h"

namespace rqs = boost::requests;
using namespace boost;

template<>
struct doctest::StringMaker<std::type_info>
{
    static String convert(const std::type_info & ti)
    {
        auto sv = boost::core::demangle(ti.name());
        return String(sv.data(), sv.size());
    }
};


TEST_SUITE_BEGIN("facade");

TEST_CASE("variadics")
{
    CHECK(rqs::detail::is_completion_token<void(int), void(*)(int)>::value);
    CHECK(!rqs::detail::is_completion_token<void(int), void(*)()>::value);
    auto l = [](int) {};
    CHECK(rqs::detail::is_completion_token<void(int), decltype(l)>::value);

    using exec = asio::deferred_t::executor_with_default<asio::any_io_executor>;

    CHECK(typeid(asio::default_completion_token_t<exec>) != typeid(void));
    CHECK(typeid(asio::default_completion_token_t<exec>) == typeid(asio::deferred));


    CHECK(typeid(rqs::detail::completion_token<void(int), exec>)                                    == typeid(asio::deferred));
    CHECK(typeid(rqs::detail::completion_token<void(int), exec, int>)                               == typeid(asio::deferred));
    CHECK(typeid(rqs::detail::completion_token<void(int), exec, decltype(l), int>)                  == typeid(asio::deferred));
    CHECK(typeid(rqs::detail::completion_token<void(int), exec, int, decltype(l)>)                  == typeid(l));
    CHECK(typeid(rqs::detail::completion_token<void(int), asio::any_io_executor, int, decltype(l)>) == typeid(l));

    auto r1 = rqs::detail::get_completion_token<void(int), exec>();
    auto r2 = rqs::detail::get_completion_token<void(int), exec>(42);
    auto r3 = rqs::detail::get_completion_token<void(int), exec>(l, 42);
    auto r4 = rqs::detail::get_completion_token<void(int), exec>(42, l);
    auto r5 = rqs::detail::get_completion_token<void(int), asio::any_io_executor>(42, l);

    CHECK(typeid(r1) == typeid(asio::deferred));
    CHECK(typeid(r2) == typeid(asio::deferred));
    CHECK(typeid(r3) == typeid(asio::deferred));
    CHECK(typeid(r4) == typeid(l));
    CHECK(typeid(r5) == typeid(l));


    CHECK(rqs::detail::tie_args<void(int), exec>() == std::make_tuple());
    CHECK(rqs::detail::tie_args<void(int), exec>(42) == std::make_tuple(42));
    CHECK(rqs::detail::tie_args<void(int), exec>(l, 42) == std::make_tuple(l, 42));
    CHECK(rqs::detail::tie_args<void(int), exec>(42, l) == std::make_tuple(42));
    CHECK(rqs::detail::tie_args<void(int), asio::any_io_executor>(42, l) == std::make_tuple(42));
}
/*
struct test_op1
{

};
TEST_CASE("Experiment")
{
    test_impl ti{};

    json::storage_ptr ptr;
    //auto res = ti.get("/api/test", ptr, test_op1{});


}*/

TEST_SUITE_END();