file(GLOB nutmeg_schema_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_schema ${nutmeg_schema_sources})
target_include_directories(nutmeg_schema PUBLIC include)
target_link_libraries(nutmeg_schema PUBLIC nutmeg_core nutmeg_warnings)

# Tests live here so the module owns its whole build. Catch2 is fetched by
# tests/, so this directory must be added after it.
FetchContent_GetProperties(Catch2 SOURCE_DIR nutmeg_catch2_dir)
include(${nutmeg_catch2_dir}/extras/Catch.cmake)

file(GLOB nutmeg_schema_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_schema_tests ${nutmeg_schema_test_sources})
target_link_libraries(nutmeg_schema_tests PRIVATE nutmeg_schema Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_schema_tests)
