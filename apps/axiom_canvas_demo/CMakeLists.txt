add_library(axiom_canvas_demo_common STATIC
    common/camera_controller.cpp
    common/demo_hit_select.cpp)
target_include_directories(axiom_canvas_demo_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(axiom_canvas_demo_common PUBLIC
    canvas_runtime_foundation canvas_runtime_scene canvas_runtime_render)
target_compile_features(axiom_canvas_demo_common PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(axiom_canvas_demo_common PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(axiom_canvas_demo_common PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(axiom_demo_camera_tests tests/camera_controller_test.cpp)
    target_link_libraries(axiom_demo_camera_tests PRIVATE axiom_canvas_demo_common)
    target_compile_features(axiom_demo_camera_tests PRIVATE cxx_std_20)
    add_test(NAME G3Camera COMMAND axiom_demo_camera_tests)

    add_executable(axiom_demo_hit_select_tests tests/demo_hit_select_test.cpp)
    target_link_libraries(axiom_demo_hit_select_tests PRIVATE axiom_canvas_demo_common)
    target_compile_features(axiom_demo_hit_select_tests PRIVATE cxx_std_20)
    add_test(NAME G3DemoHitSelect COMMAND axiom_demo_hit_select_tests)

    foreach(target axiom_demo_camera_tests axiom_demo_hit_select_tests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /WX /EHsc)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    endforeach()
endif()
