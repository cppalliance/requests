


file(GLOB_RECURSE ALL_TEST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(tests ${ALL_TEST_FILES} ../include/boost/requests/service.hpp ../include/boost/requests/request.hpp ../include/boost/requests/impl/request.hpp ../include/boost/requests/impl/request_options.ipp)
target_link_libraries(tests PUBLIC Threads::Threads OpenSSL::Crypto OpenSSL::SSL Boost::headers Boost::container rt)
target_compile_definitions(tests PUBLIC BOOST_REQUESTS_SEPARATE_COMPILATION=1 BOOST_REQUESTS_USE_STD_FS=1)