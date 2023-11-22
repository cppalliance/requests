enable_testing()

add_executable(boost_requests_unit_tests
        main_test.cpp
        fixtures/server.cpp
        unit/detail/condition_variable.cpp
        unit/connection.cpp
        unit/connection_pool.cpp
        unit/cookie_grammar.cpp
        unit/cookie_jar.cpp
        unit/fixed_token_rule.cpp
        unit/form.cpp
        unit/keep_alive.cpp
        unit/link.cpp
        unit/quoted_string.cpp
        unit/raw_string.cpp
        unit/redirect.cpp
        unit/request_stream.cpp
        unit/rfc.cpp
        unit/service.cpp
        unit/session.cpp
        unit/source.cpp
        unit/token_rule.cpp
)

target_link_libraries(boost_requests_unit_tests PUBLIC
        Boost::requests Boost::filesystem Boost::context Boost::unit_test_framework rt)

add_test(NAME boost_requests_unit_tests COMMAND boost_requests_unit_tests)


add_executable(boost_requests_httpbin_tests
        main_test.cpp
        fixtures/httpbin.cpp
        httpbin/cookie.cpp
        httpbin/json.cpp
        httpbin/method.cpp
)

target_link_libraries(boost_requests_httpbin_tests PUBLIC
        Boost::requests Boost::filesystem Boost::context Boost::unit_test_framework rt)

add_test(NAME boost_requests_httpbin_tests COMMAND boost_requests_httpbin_tests)

