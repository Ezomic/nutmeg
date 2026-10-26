file(GLOB nutmeg_sim_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_sim ${nutmeg_sim_sources})
target_include_directories(nutmeg_sim PUBLIC include)
target_link_libraries(nutmeg_sim PUBLIC nutmeg_platform nutmeg_core nutmeg_warnings)

include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

file(GLOB nutmeg_sim_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_sim_tests ${nutmeg_sim_test_sources})
target_link_libraries(nutmeg_sim_tests PRIVATE nutmeg_sim Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_sim_tests)
