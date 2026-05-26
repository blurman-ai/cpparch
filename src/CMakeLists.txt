# Core library — INTERFACE for v0.1 (no implementation files yet).
# Subsystems (config/, scan/, graph/, rules/, report/) attach here as they land.
add_library(archcheck_core INTERFACE)
add_library(archcheck::core ALIAS archcheck_core)

target_include_directories(archcheck_core INTERFACE
   ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(archcheck_core INTERFACE ryml::ryml)

target_compile_features(archcheck_core INTERFACE cxx_std_20)

target_compile_definitions(archcheck_core INTERFACE
   ARCHCHECK_VERSION_STRING="${PROJECT_VERSION}"
   ARCHCHECK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
   ARCHCHECK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
   ARCHCHECK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# CLI entry point.
add_executable(archcheck main.cpp)
target_link_libraries(archcheck PRIVATE archcheck::core)
