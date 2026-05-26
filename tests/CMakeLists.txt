# Catch2 v3 — header-only-ish, simple integration via FetchContent.
include(FetchContent)
FetchContent_Declare(
   Catch2
   GIT_REPOSITORY https://github.com/catchorg/Catch2.git
   GIT_TAG v3.7.1
   GIT_SHALLOW TRUE
)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(archcheck_tests
   smoke_test.cpp
)
target_link_libraries(archcheck_tests
   PRIVATE
      archcheck::core
      Catch2::Catch2WithMain
)

catch_discover_tests(archcheck_tests)
