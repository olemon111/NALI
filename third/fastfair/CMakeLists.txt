cmake_minimum_required(VERSION 3.14)
include(FetchContent)
project(confastfair)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -lpmem -lpmemobj -ljemalloc -mrtm -pthread")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_definitions(-DNDEBUG)
message(STATUS "NDEBUG: defined")

add_library(nali_fastfair SHARED con_fastfair.cc con_fastfair.h)

