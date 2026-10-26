file(GLOB nutmeg_physics_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_physics ${nutmeg_physics_sources})
target_include_directories(nutmeg_physics PUBLIC include)
target_link_libraries(nutmeg_physics PUBLIC nutmeg_core nutmeg_warnings)

FetchContent_GetProperties(Catch2 SOURCE_DIR nutmeg_catch2_source)
list(APPEND CMAKE_MODULE_PATH ${nutmeg_catch2_source}/extras)
include(Catch)

file(GLOB nutmeg_physics_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_physics_tests ${nutmeg_physics_test_sources})
target_link_libraries(nutmeg_physics_tests PRIVATE nutmeg_physics Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_physics_tests)
