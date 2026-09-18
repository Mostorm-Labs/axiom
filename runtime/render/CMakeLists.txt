option(CANVAS_RENDER_ENABLE_SKIA_HEADLESS
    "Build the private G3 Headless Skia backend" OFF)

add_library(canvas_runtime_render STATIC
    src/visibility_resolver.cpp
    src/direct_reference_source.cpp
    src/frame_plan.cpp
    src/render_backend.cpp)

if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    if(NOT CANVAS_SKIA_SDK_ROOT)
        message(FATAL_ERROR "CANVAS_SKIA_SDK_ROOT is required for the Headless Skia consumer")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${CANVAS_SKIA_SDK_ROOT}")
    find_package(CanvasSkia CONFIG REQUIRED)
    target_sources(canvas_runtime_render PRIVATE src/skia_headless_backend.cpp)
    target_link_libraries(canvas_runtime_render PRIVATE CanvasSkia::Skia)
endif()
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

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_render_headless_skia_tests
        tests/skia_headless_backend_test.cpp)
    target_link_libraries(axiom_render_headless_skia_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_headless_skia_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_headless_skia_tests
            PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_render_headless_skia_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G3HeadlessSkia COMMAND axiom_render_headless_skia_tests)
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

    add_executable(axiom_render_direct_reference_tests
        tests/direct_reference_source_test.cpp)
    target_link_libraries(axiom_render_direct_reference_tests
        PRIVATE canvas_runtime_render canvas_runtime_scene canvas_runtime_semantic)
    target_include_directories(axiom_render_direct_reference_tests
        PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
    target_compile_features(axiom_render_direct_reference_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(axiom_render_direct_reference_tests
            PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_render_direct_reference_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME G3DirectReference COMMAND axiom_render_direct_reference_tests)

    add_executable(axiom_render_frame_orchestration_tests
        tests/frame_orchestration_test.cpp)
    target_link_libraries(axiom_render_frame_orchestration_tests
        PRIVATE canvas_runtime_render canvas_runtime_scene canvas_runtime_semantic)
    target_compile_features(axiom_render_frame_orchestration_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(axiom_render_frame_orchestration_tests
            PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_render_frame_orchestration_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME G3FrameOrchestration COMMAND axiom_render_frame_orchestration_tests)
endif()
