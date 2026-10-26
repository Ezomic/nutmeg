file(GLOB nutmeg_gameplay_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_gameplay ${nutmeg_gameplay_sources})
target_include_directories(nutmeg_gameplay PUBLIC include)
target_link_libraries(nutmeg_gameplay PUBLIC nutmeg_physics nutmeg_schema nutmeg_controls nutmeg_core nutmeg_warnings)

FetchContent_GetProperties(Catch2 SOURCE_DIR nutmeg_catch2_source)
list(APPEND CMAKE_MODULE_PATH ${nutmeg_catch2_source}/extras)
include(Catch)

file(GLOB nutmeg_gameplay_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_gameplay_tests ${nutmeg_gameplay_test_sources})
target_link_libraries(nutmeg_gameplay_tests PRIVATE nutmeg_gameplay Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_gameplay_tests)
