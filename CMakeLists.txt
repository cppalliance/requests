cmake_minimum_required(VERSION 3.20)
project(requests)

find_package(Boost REQUIRED system container filesystem atomic)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

include_directories(include libs/url/include)

add_subdirectory(test)
add_subdirectory(example)
