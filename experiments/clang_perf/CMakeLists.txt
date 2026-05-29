cmake_minimum_required(VERSION 3.18)
project(clang_perf_spike CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Spike is throw-away — keep it loud about its purpose.
message(STATUS "[spike #043] clang_perf — libclang-only backend feasibility check")

# We only need the libclang C API — no need to drag in find_package(Clang)
# (which picks llvm-11 ClangConfig on this host and breaks). Look up the
# header + .so directly.
find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    HINTS /usr/lib/llvm-19/include
    NO_DEFAULT_PATH
)
find_library(LIBCLANG_LIBRARY NAMES clang
    HINTS /usr/lib/llvm-19/lib
    NO_DEFAULT_PATH
)

if (NOT LIBCLANG_INCLUDE_DIR OR NOT LIBCLANG_LIBRARY)
    message(FATAL_ERROR "[spike #043] libclang dev not found — install libclang-19-dev")
endif()

message(STATUS "[spike #043] libclang headers: ${LIBCLANG_INCLUDE_DIR}")
message(STATUS "[spike #043] libclang library: ${LIBCLANG_LIBRARY}")

add_executable(clang_perf main.cpp)
target_include_directories(clang_perf PRIVATE ${LIBCLANG_INCLUDE_DIR})
target_link_libraries(clang_perf PRIVATE ${LIBCLANG_LIBRARY})

add_executable(regex_baseline regex_baseline.cpp)
