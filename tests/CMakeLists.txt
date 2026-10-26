include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

file(GLOB nutmeg_test_sources CONFIGURE_DEPENDS test_*.cpp)
add_executable(nutmeg_tests ${nutmeg_test_sources})
target_link_libraries(nutmeg_tests PRIVATE nutmeg_platform Catch2::Catch2WithMain)
catch_discover_tests(nutmeg_tests)
