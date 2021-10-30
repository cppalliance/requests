cmake_minimum_required(VERSION 3.20)
project(requests)

find_package(Boost REQUIRED system )
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)

include_directories(include libs/url/include)

add_subdirectory(test)
