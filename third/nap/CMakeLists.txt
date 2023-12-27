cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project(Nap)

set(CMAKE_CXX_COMPILER "g++")

option(ENABLE_NAP_FLAG "Enable Nap" ON) 
option(RANGE_BENCH_FLAG "Enable Range " OFF) 
option(TEST_LATENCY_FLAG "Enable Lantecy Test " OFF) 
option(SWITCH_TEST_FLAG "Enable Switch Test " OFF)
option(USE_GLOBAL_LOCK_FLAG "Enable Switch Global Lock Test " OFF) 
option(RECOVERY_TEST_FLAG "Enable Recovery Test " OFF) 

# set(CMAKE_C_FLAGS "-Wall -Wsign-compare -O3 -DNDEBUG")
set(CMAKE_C_FLAGS "-Wall -march=native -Wsign-compare -O3 -g")

if(ENABLE_NAP_FLAG)
string(APPEND CMAKE_C_FLAGS " -DENABLE_NAP")
endif(ENABLE_NAP_FLAG)

if(RANGE_BENCH_FLAG)
string(APPEND CMAKE_C_FLAGS " -DRANGE_BENCH -DSUPPORT_RANGE")
endif(RANGE_BENCH_FLAG)

if(TEST_LATENCY_FLAG)
string(APPEND CMAKE_C_FLAGS " -DTEST_LATENCY")
endif(TEST_LATENCY_FLAG)

if(SWITCH_TEST_FLAG)
string(APPEND CMAKE_C_FLAGS " -DSWITCH_TEST")
endif(SWITCH_TEST_FLAG)

if(USE_GLOBAL_LOCK_FLAG)
string(APPEND CMAKE_C_FLAGS " -DUSE_GLOBAL_LOCK")
endif(USE_GLOBAL_LOCK_FLAG)

if(RECOVERY_TEST_FLAG)
string(APPEND CMAKE_C_FLAGS " -DRECOVERY_TEST")
endif(RECOVERY_TEST_FLAG)


#Compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -march=native ")

# Link Options
set(LINKS_FLAGS "-lpthread -lpmemobj -ltbb -ljemalloc -lprofiler")

option (SANITIZE "Turn on sanitization" OFF)
if (SANITIZE)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

#Incs and Libs
set(INCLUDE_BASE ${PROJECT_SOURCE_DIR}/include)
include_directories(${INCLUDE_BASE})

#Source file define
set(COMMON_SRC ${PROJECT_SOURCE_DIR}/src)

#Used by both server and clients
file(GLOB_RECURSE COMMON_FILE ${COMMON_SRC}/*.cpp)
add_library(nali_nap SHARED ${COMMON_FILE})

