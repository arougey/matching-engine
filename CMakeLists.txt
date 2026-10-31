cmake_minimum_required(VERSION 3.20)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_library(orderbook src/OrderBook.cpp)
target_include_directories(orderbook PUBLIC include)

add_executable(demo src/main.cpp)
target_link_libraries(demo PRIVATE orderbook)

option(ORDERBOOK_BUILD_TESTS "Build unit tests" ON)
if(NOT ORDERBOOK_BUILD_TESTS)
  return()
endif()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

enable_testing()
add_executable(orderbook_tests tests/OrderBookTests.cpp)
target_link_libraries(orderbook_tests PRIVATE orderbook GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(orderbook_tests)
