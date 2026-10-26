file(GLOB nutmeg_core_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_core ${nutmeg_core_sources})
target_include_directories(nutmeg_core PUBLIC include)
target_link_libraries(nutmeg_core PUBLIC nutmeg_warnings)
