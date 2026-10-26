add_library(nutmeg_debugview_model src/view_transform.cpp src/demo_scene.cpp)
target_include_directories(nutmeg_debugview_model PUBLIC include)
target_link_libraries(nutmeg_debugview_model PUBLIC nutmeg_warnings)

if(NUTMEG_RUNTIME)
    add_library(nutmeg_debugview src/debug_renderer.cpp)
    target_link_libraries(nutmeg_debugview PUBLIC nutmeg_debugview_model nutmeg_core PRIVATE SDL3::SDL3)

    add_executable(nutmeg-debugview src/main.cpp)
    target_link_libraries(nutmeg-debugview PRIVATE nutmeg_debugview nutmeg_platform_sdl SDL3::SDL3)
endif()
