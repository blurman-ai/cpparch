# Core library — STATIC, накапливает реализацию подсистем.
add_library(archcheck_core STATIC
   scan/include_scanner.cpp
   scan/project_files.cpp
   scan/include_resolver.cpp
   scan/disk_file_source.cpp
   graph/dependency_graph.cpp
   graph/algorithms.cpp
   graph/diff.cpp
   graph/baseline.cpp
   graph/graph_builder.cpp
   git/git_state.cpp
   git/git_object_file_source.cpp
   diff/regression_report.cpp
   report/json_reporter.cpp
   report/text_reporter.cpp
   rules/lakos_chain_length.cpp
   rules/lakos_god_headers.cpp
   rules/sf7_using_namespace.cpp
   rules/sf8_include_guard.cpp
   rules/sf9_no_cycles.cpp
   rules/rule_set.cpp
)
add_library(archcheck::core ALIAS archcheck_core)

target_include_directories(archcheck_core PUBLIC
   ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(archcheck_core PUBLIC ryml::ryml)

if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
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
