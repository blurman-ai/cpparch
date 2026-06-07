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
   duplication_token_normalizer_test.cpp
   duplication_similarity_test.cpp
   duplication_clone_classifier_test.cpp
   duplication_fragmenter_test.cpp
   duplication_large_function_test.cpp
   duplication_clone_index_test.cpp
   duplication_fp_guards_test.cpp
   duplication_path_guards_test.cpp
   # Research/QA precision-evaluation harness — test-only, deliberately NOT in
   # the shipped archcheck_core library (#084). Compiled straight into the tests.
   ${CMAKE_SOURCE_DIR}/src/scan/duplication/fp_corpus_eval.cpp
   duplication_fp_corpus_eval_test.cpp
   # duplication_vmecpp_test.cpp / duplication_all_projects_test.cpp — research
   # harnesses that read hardcoded local corpus paths (sandbox/, drift_repos/);
   # not hermetic, cannot run on CI. Build/run manually when the corpora exist.
   duplication_synthetic_fp_test.cpp
   duplication_qt_slots_test.cpp
   duplication_qt_p06_test.cpp
   duplication_clone_type_wiring_test.cpp
   unit/scan/include_scanner_test.cpp
   unit/scan/project_files_test.cpp
   unit/scan/include_resolver_test.cpp
   unit/scan/file_classification_test.cpp
   unit/graph/dependency_graph_test.cpp
   unit/graph/algorithms_test.cpp
   unit/graph/diff_test.cpp
   unit/graph/baseline_test.cpp
   unit/git/git_state_test.cpp
   unit/git/git_object_file_source_test.cpp
   unit/diff/regression_report_test.cpp
   unit/report/json_escape_test.cpp
   unit/report/violation_baseline_test.cpp
   integration/scan/scanner_fixtures_test.cpp
   integration/graph/end_to_end_test.cpp
   integration/graph/vendor_exclude_test.cpp
   unit/rules/drift_no_shortcut_edge_test.cpp
   unit/rules/drift_no_cycle_growth_test.cpp
   unit/rules/lakos_chain_length_test.cpp
   unit/rules/lakos_god_headers_test.cpp
   unit/rules/sf7_using_namespace_test.cpp
   unit/rules/sf8_include_guard_test.cpp
   unit/rules/sf9_no_cycles_test.cpp
   integration/diff/git_diff_test.cpp
   integration/rules/drift_fixtures_test.cpp
   unit/config/test_loader.cpp
)
target_link_libraries(archcheck_tests
   PRIVATE
      archcheck::core
      Catch2::Catch2WithMain
)
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_libraries(archcheck_tests PRIVATE stdc++fs)
endif()
target_compile_definitions(archcheck_tests PRIVATE
   ARCHCHECK_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/fixtures"
)

catch_discover_tests(archcheck_tests)
