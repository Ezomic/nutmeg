file(GLOB nutmeg_platform_sources CONFIGURE_DEPENDS src/*.cpp)
add_library(nutmeg_platform ${nutmeg_platform_sources})
target_include_directories(nutmeg_platform PUBLIC include)
target_link_libraries(nutmeg_platform PUBLIC nutmeg_core nutmeg_warnings)
