add_library(canvas_runtime_render STATIC
    src/visibility_resolver.cpp)
target_include_directories(canvas_runtime_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(canvas_runtime_render PUBLIC
    canvas_runtime_foundation canvas_runtime_semantic canvas_runtime_scene)
target_compile_features(canvas_runtime_render PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(canvas_runtime_render PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_runtime_render PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(axiom_render_frame_state_tests
        tests/frame_state_contract_test.cpp)
    target_link_libraries(axiom_render_frame_state_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_frame_state_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(axiom_render_frame_state_tests
            PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_render_frame_state_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME G3FrameState COMMAND axiom_render_frame_state_tests)

    add_executable(axiom_render_visibility_tests
        tests/visibility_resolver_test.cpp)
    target_link_libraries(axiom_render_visibility_tests
        PRIVATE canvas_runtime_render canvas_runtime_scene)
    target_include_directories(axiom_render_visibility_tests
        PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
    target_compile_features(axiom_render_visibility_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(axiom_render_visibility_tests
            PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_render_visibility_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME G3Visibility COMMAND axiom_render_visibility_tests)
endif()
