


file(GLOB_RECURSE ALL_TEST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(tests ${ALL_TEST_FILES})
target_link_libraries(tests PUBLIC Threads::Threads OpenSSL::Crypto OpenSSL::SSL Boost::headers rt)
target_compile_definitions(tests PUBLIC BOOST_REQUESTS_SEPARATE_COMPILATION=1)