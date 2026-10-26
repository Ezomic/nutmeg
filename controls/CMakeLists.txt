file(GLOB nutmeg_controls_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_controls ${nutmeg_controls_sources})
target_include_directories(nutmeg_controls PUBLIC include)
target_link_libraries(nutmeg_controls PUBLIC nutmeg_core nutmeg_warnings)

FetchContent_GetProperties(Catch2 SOURCE_DIR nutmeg_catch2_source)
list(APPEND CMAKE_MODULE_PATH ${nutmeg_catch2_source}/extras)
include(Catch)

file(GLOB nutmeg_controls_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_controls_tests ${nutmeg_controls_test_sources})
target_link_libraries(nutmeg_controls_tests PRIVATE nutmeg_controls Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_controls_tests)
