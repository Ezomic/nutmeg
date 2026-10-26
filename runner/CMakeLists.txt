include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB nutmeg_runner_sources CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM nutmeg_runner_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
add_library(nutmeg_runner ${nutmeg_runner_sources})
target_include_directories(nutmeg_runner PUBLIC include)
target_link_libraries(nutmeg_runner PUBLIC nutmeg_core nutmeg_warnings nlohmann_json::nlohmann_json)

add_executable(nutmeg-run src/main.cpp)
target_link_libraries(nutmeg-run PRIVATE nutmeg_runner)

if(NOT COMMAND catch_discover_tests)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

file(GLOB nutmeg_runner_test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(nutmeg_runner_tests ${nutmeg_runner_test_sources})
target_link_libraries(nutmeg_runner_tests PRIVATE nutmeg_runner Catch2::Catch2WithMain)
target_compile_definitions(nutmeg_runner_tests PRIVATE NUTMEG_RUNNER_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
catch_discover_tests(nutmeg_runner_tests)
