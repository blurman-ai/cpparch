# Core library — STATIC, накапливает реализацию подсистем.
add_library(archcheck_core STATIC
   scan/include_scanner.cpp
   scan/project_files.cpp
   scan/include_resolver.cpp
   scan/disk_file_source.cpp
   scan/satd_scan.cpp
   scan/test_co_evolution.cpp
   scan/god_file_growth.cpp
   scan/defect_attractor.cpp
   scan/function_body_scan.cpp
   scan/local_complexity_metrics.cpp
   scan/local_complexity_drift.cpp
   scan/duplication/token_normalizer.cpp
   scan/duplication/fragmenter.cpp
   scan/duplication/similarity.cpp
   scan/duplication/clone_classifier.cpp
   scan/duplication/clone_index.cpp
   scan/duplication/duplication_scanner.cpp
   # NB: scan/duplication/fp_corpus_eval.cpp is NOT here — it is a research/QA
   # precision-evaluation harness, compiled into archcheck_tests only (see
   # tests/CMakeLists.txt), not into the shipped archcheck_core library. (#084)
   graph/dependency_graph.cpp
   graph/algorithms.cpp
   graph/diff.cpp
   graph/baseline.cpp
   graph/graph_builder.cpp
   git/git_exec.cpp
   git/git_state.cpp
   git/git_object_file_source.cpp
   git/diff_query.cpp
   git/history_query.cpp
   diff/regression_report.cpp
   report/json_reporter.cpp
   report/text_reporter.cpp
   report/violation_baseline.cpp
   rules/drift_no_shortcut_edge.cpp
   rules/drift_no_cycle_growth.cpp
   rules/drift_bidirectional_coupling.cpp
   rules/lakos_chain_length.cpp
   rules/lakos_god_headers.cpp
   rules/sf7_using_namespace.cpp
   rules/sf8_include_guard.cpp
   rules/sf9_no_cycles.cpp
   rules/rule_set.cpp
   config/config_loader.cpp
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

# CLI entry point: thin dispatch in main.cpp + application command units in cli/.
add_executable(archcheck
   main.cpp
   cli/check_command.cpp
   cli/diff_command.cpp
   cli/preview_commands.cpp
)
target_include_directories(archcheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(archcheck PRIVATE archcheck::core)
