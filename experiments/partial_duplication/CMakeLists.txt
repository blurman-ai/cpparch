cmake_minimum_required(VERSION 3.18)
project(partial_duplication_spike CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "[spike] partial_duplication — standalone Type-3 token-overlap detector")

add_executable(partial_duplication main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(partial_duplication PRIVATE stdc++fs)
endif()
