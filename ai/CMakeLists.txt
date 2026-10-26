file(GLOB nutmeg_ai_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_ai ${nutmeg_ai_sources})
target_include_directories(nutmeg_ai PUBLIC include)
target_link_libraries(nutmeg_ai PUBLIC nutmeg_gameplay nutmeg_physics nutmeg_core nutmeg_warnings)

FetchContent_GetProperties(Catch2 SOURCE_DIR nutmeg_catch2_source)
list(APPEND CMAKE_MODULE_PATH ${nutmeg_catch2_source}/extras)
include(Catch)

file(GLOB nutmeg_ai_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_ai_tests ${nutmeg_ai_test_sources})
target_link_libraries(nutmeg_ai_tests PRIVATE nutmeg_ai Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_ai_tests)
