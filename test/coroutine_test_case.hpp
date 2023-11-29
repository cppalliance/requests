//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_ASYNC_TEST_CASE_HPP
#define BOOST_REQUESTS_ASYNC_TEST_CASE_HPP

#include <boost/test/unit_test.hpp>
#include <boost/asio/spawn.hpp>

#define BOOST_FIXTURE_COROUTINE_TEST_CASE_WITH_DECOR( test_name, F, decorators )      \
struct test_name : public F { void test_method(boost::asio::yield_context yield ); }; \
                                                                                      \
static void BOOST_AUTO_TC_INVOKER( test_name )()                                      \
{                                                                                     \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" fixture ctor");                    \
    test_name t;                                                                      \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" fixture setup");                   \
    boost::unit_test::setup_conditional(t);                                           \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" test entry");                      \
    boost::asio::io_context ctx{1};                                                   \
    boost::asio::spawn(ctx,                                                           \
                       [&t](boost::asio::yield_context yield)                         \
                       {                                                              \
                          t.test_method(std::move(yield));                            \
                       },                                                             \
                       [](std::exception_ptr ep)                                      \
                       {                                                              \
                          if (ep)                                                     \
                            std::rethrow_exception(ep);                               \
                       });                                                            \
                                                                                      \
    ctx.run();                                                                        \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" fixture teardown");                \
    boost::unit_test::teardown_conditional(t);                                        \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" fixture dtor");                    \
}                                                                                     \
                                                                                      \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name ) {};                                       \
                                                                                      \
BOOST_AUTO_TU_REGISTRAR( test_name )(                                                 \
    boost::unit_test::make_test_case(                                                 \
        &BOOST_AUTO_TC_INVOKER( test_name ),                                          \
        #test_name, __FILE__, __LINE__ ),                                             \
        decorators );                                                                 \
                                                                                      \
void test_name::test_method(boost::asio::yield_context yield) /**/

#define BOOST_FIXTURE_COROUTINE_TEST_CASE_NO_DECOR( test_name, F )                    \
BOOST_FIXTURE_COROUTINE_TEST_CASE_WITH_DECOR( test_name, F,                           \
    boost::unit_test::decorator::collector_t::instance() )                            \
/**/

#define BOOST_FIXTURE_COROUTINE_TEST_CASE( ... )                                      \
    BOOST_TEST_INVOKE_IF_N_ARGS( 2,                                                   \
        BOOST_FIXTURE_COROUTINE_TEST_CASE_NO_DECOR,                                   \
        BOOST_FIXTURE_COROUTINE_TEST_CASE_WITH_DECOR,                                 \
         __VA_ARGS__)                                                                 \

#define BOOST_COROUTINE_TEST_CASE_NO_DECOR( test_name )                               \
    BOOST_FIXTURE_COROUTINE_TEST_CASE_NO_DECOR( test_name,                            \
        BOOST_AUTO_TEST_CASE_FIXTURE )                                                \
/**/

#define BOOST_COROUTINE_TEST_CASE_WITH_DECOR( test_name, decorators )                 \
    BOOST_FIXTURE_COROUTINE_TEST_CASE_WITH_DECOR( test_name,                          \
        BOOST_COROUTINE_TEST_CASE_FIXTURE, decorators )                               \
/**/

#define BOOST_COROUTINE_TEST_CASE( ... )                                              \
    BOOST_TEST_INVOKE_IF_N_ARGS( 1,                                                   \
        BOOST_COROUTINE_TEST_CASE_NO_DECOR,                                           \
        BOOST_COROUTINE_TEST_CASE_WITH_DECOR,                                         \
         __VA_ARGS__)                                                                 \


#endif // BOOST_REQUESTS_ASYNC_TEST_CASE_HPP
