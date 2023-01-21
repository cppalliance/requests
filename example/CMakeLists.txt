
file(GLOB_RECURSE ALL_EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

include(FetchContent)
FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        master)

FetchContent_MakeAvailable(pugixml)


add_library(boost_requests_example_impl impl.cpp)
target_compile_definitions(boost_requests_example_impl PUBLIC
        BOOST_REQUESTS_USE_STD_FS=1
        BOOST_REQUESTS_SEPARATE_COMPILATION=1
        BOOST_BEAST_SEPARATE_COMPILATION=1
        BOOST_ASIO_SEPARATE_COMPILATION=1
        BOOST_FILESYSTEM_NO_DEPRECATED=1)
target_link_libraries(boost_requests_example_impl  PUBLIC
        Threads::Threads OpenSSL::Crypto OpenSSL::SSL
        Boost::headers Boost::container rt pugixml::static)


foreach(SRC ${ALL_EXAMPLES})
    if (SRC STREQUAL "impl.cpp")
        continue()
    endif()

    get_filename_component(NAME ${SRC} NAME_WLE )
    add_executable(boost_requests_example_${NAME} ${SRC} )

    target_link_libraries(boost_requests_example_${NAME}  PUBLIC boost_requests_example_impl)
    target_compile_definitions(boost_requests_example_${NAME} PUBLIC
            BOOST_REQUESTS_SEPARATE_COMPILATION=1
            BOOST_REQUESTS_USE_STD_FS=1
            BOOST_BEAST_SEPARATE_COMPILATION=1
            BOOST_ASIO_SEPARATE_COMPILATION=1)

endforeach()
