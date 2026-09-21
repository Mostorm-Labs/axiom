option(CANVAS_RENDER_ENABLE_SKIA_HEADLESS
    "Build the private G3 Headless Skia backend" OFF)
option(CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH
    "Build the shared Skia programmable-DAB renderer" OFF)

add_library(canvas_runtime_render STATIC
    src/visibility_resolver.cpp
    src/direct_reference_source.cpp
    src/frame_plan.cpp
    src/render_backend.cpp
    src/surface_lifecycle.cpp
    src/presentation_tracker.cpp
    src/canonical_handoff.cpp)

if(EMSCRIPTEN AND (CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR
                   CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH))
    target_sources(canvas_runtime_render PRIVATE src/webgl_surface_backend.cpp)
endif()

if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
    if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
      add_executable(axiom_headless_canvas_demo
        tools/headless_canvas_demo.cpp)
      target_link_libraries(axiom_headless_canvas_demo PRIVATE canvas_runtime_render)
      target_compile_features(axiom_headless_canvas_demo PRIVATE cxx_std_20)
      target_compile_definitions(axiom_headless_canvas_demo PRIVATE
        G3_07_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    endif()
endif()

if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
    if(NOT CANVAS_SKIA_SDK_ROOT)
        message(FATAL_ERROR "CANVAS_SKIA_SDK_ROOT is required for the Headless Skia consumer")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${CANVAS_SKIA_SDK_ROOT}")
    find_package(CanvasSkia CONFIG REQUIRED
        PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH)
    if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
      target_sources(canvas_runtime_render PRIVATE src/skia_headless_backend.cpp)
    endif()
    target_sources(canvas_runtime_render PRIVATE src/skia_ink_backend.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_brush_renderer.cpp)
    target_link_libraries(canvas_runtime_render PRIVATE CanvasSkia::Skia)
    target_compile_definitions(canvas_runtime_render PUBLIC CANVAS_RENDER_HAS_SKIA=1)
endif()
target_include_directories(canvas_runtime_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(canvas_runtime_render PUBLIC
    canvas_runtime_foundation canvas_runtime_semantic canvas_runtime_scene
    canvas_runtime_ink)
target_compile_features(canvas_runtime_render PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(canvas_runtime_render PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_runtime_render PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(EMSCRIPTEN)
        target_compile_options(canvas_runtime_render PRIVATE
            -Wno-missing-designated-field-initializers)
    endif()
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_render_skia_ink_backend_tests
        tests/skia_ink_backend_test.cpp)
    target_link_libraries(axiom_render_skia_ink_backend_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_skia_ink_backend_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_skia_ink_backend_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_skia_ink_backend_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4WindowsSkiaInkViewport
        COMMAND axiom_render_skia_ink_backend_tests)
    add_executable(axiom_render_skia_brush_primitive_tests
        tests/skia_brush_primitive_test.cpp)
    target_link_libraries(axiom_render_skia_brush_primitive_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_skia_brush_primitive_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_skia_brush_primitive_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_skia_brush_primitive_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4SkiaBrushPrimitiveDab COMMAND axiom_render_skia_brush_primitive_tests)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_render_headless_canvas_demo_tests
        tests/headless_canvas_demo_test.cpp)
    target_link_libraries(axiom_render_headless_canvas_demo_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_headless_canvas_demo_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_render_headless_canvas_demo_tests PRIVATE
        G3_07_DEMO_BINARY="$<TARGET_FILE:axiom_headless_canvas_demo>"
        G3_07_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    add_dependencies(axiom_render_headless_canvas_demo_tests axiom_headless_canvas_demo)
    add_test(NAME G3HeadlessCanvasDemo COMMAND axiom_render_headless_canvas_demo_tests)

    add_executable(axiom_render_headless_skia_tests
        tests/skia_headless_backend_test.cpp)
    target_link_libraries(axiom_render_headless_skia_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_headless_skia_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_headless_skia_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_headless_skia_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G3HeadlessSkia COMMAND axiom_render_headless_skia_tests)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(axiom_render_surface_presentation_tests
        tests/surface_presentation_test.cpp)
    target_link_libraries(axiom_render_surface_presentation_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_surface_presentation_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(axiom_render_surface_presentation_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_surface_presentation_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME G3SurfacePresentation COMMAND axiom_render_surface_presentation_tests)

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

    add_executable(axiom_canonical_handoff_tests tests/canonical_handoff_test.cpp)
    target_link_libraries(axiom_canonical_handoff_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_canonical_handoff_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_canonical_handoff_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_canonical_handoff_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4CanonicalHandoff COMMAND axiom_canonical_handoff_tests)
endif()
