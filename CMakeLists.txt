cmake_minimum_required(VERSION 3.18)

project(archcheck
   VERSION 0.1.0
   DESCRIPTION "Architecture rules for C++ projects, enforced in CI"
   HOMEPAGE_URL "https://github.com/blurman-ai/archcheck"
   LANGUAGES CXX
)

# C++20 baseline, no compiler extensions.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type when none is set explicitly (single-config generators).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Always export compile_commands.json — archcheck itself can dogfood it later.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# Options.
option(ARCHCHECK_BUILD_TESTS "Build archcheck tests" ON)
option(ARCHCHECK_ENABLE_WARNINGS "Enable strict warning flags" ON)

# Warnings — strict by default.
if(ARCHCHECK_ENABLE_WARNINGS)
   if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow)
   elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      add_compile_options(/W4 /WX)
   endif()
endif()

# Dependencies — fetched at configure time, no system installs required.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ryml — YAML parser. Header-only-ish, low-allocation, no Boost.
# Spec §«Технологический стек» / §«Зависимости — минимум».
FetchContent_Declare(
   ryml
   GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
   GIT_TAG v0.7.0
   GIT_SHALLOW TRUE
)
set(RYML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RYML_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(RYML_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(RYML_DEFAULT_CALLBACKS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ryml)

# Project layout — see docs/architecture-spec.md §«Высокоуровневая структура кода».
add_subdirectory(src)

if(ARCHCHECK_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()
