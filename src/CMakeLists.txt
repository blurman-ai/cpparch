# Core library — STATIC, накапливает реализацию подсистем.
add_library(archcheck_core STATIC
   scan/include_scanner.cpp
   scan/project_files.cpp
   graph/dependency_graph.cpp
)
add_library(archcheck::core ALIAS archcheck_core)

target_include_directories(archcheck_core PUBLIC
   ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(archcheck_core PUBLIC ryml::ryml)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
   target_link_libraries(archcheck_core PUBLIC stdc++fs)
endif()

target_compile_features(archcheck_core PUBLIC cxx_std_20)

target_compile_definitions(archcheck_core PUBLIC
   ARCHCHECK_VERSION_STRING="${PROJECT_VERSION}"
   ARCHCHECK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
   ARCHCHECK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
   ARCHCHECK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# CLI entry point.
add_executable(archcheck main.cpp)
target_link_libraries(archcheck PRIVATE archcheck::core)
