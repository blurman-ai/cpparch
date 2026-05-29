cmake_minimum_required(VERSION 3.18)
project(line_duplication_spike CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "[spike] line_duplication — standalone Type-1 copy-paste detector")

add_executable(line_duplication
  main.cpp
  ../../src/git/git_object_file_source.cpp
  ../../src/git/git_state.cpp
  ../../src/scan/project_files.cpp
)

target_include_directories(line_duplication PRIVATE ../../include)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(line_duplication PRIVATE stdc++fs)
endif()
