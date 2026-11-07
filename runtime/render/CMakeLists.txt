option(CANVAS_RENDER_ENABLE_SKIA_HEADLESS
    "Build the private G3 Headless Skia backend" OFF)
option(CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH
    "Build the shared Skia programmable-DAB renderer" OFF)

add_library(canvas_runtime_render STATIC
    src/damage_tracker.cpp
    src/render_group_manager.cpp
    src/selection_region_geometry.cpp
    src/visibility_resolver.cpp
    src/direct_reference_source.cpp
    src/frame_plan.cpp
    src/editing_overlay.cpp
    src/render_backend.cpp
    src/surface_lifecycle.cpp
    src/render_target.cpp
    src/runtime_app_binding.cpp
    src/runtime_frame_coordinator.cpp
    src/presentation_tracker.cpp
    src/canonical_handoff.cpp
    src/preview_surface.cpp)
target_link_libraries(canvas_runtime_render PUBLIC canvas_runtime_scheduling canvas_runtime_resources)

# Private source-build comparison profiles. These do not add a consumer API or
# alter the installed JS/DLL contract. Keep the accepted default at32/96MiB.
set(AXIOM_RENDER_CACHE_MIB "32" CACHE STRING "Optional derived render cache MiB")
set(AXIOM_RENDER_SHARED_MIB "96" CACHE STRING "Shared derived resource hard cap MiB")
foreach(_budget AXIOM_RENDER_CACHE_MIB AXIOM_RENDER_SHARED_MIB)
    if(NOT "${${_budget}}" MATCHES "^[1-9][0-9]*$" OR ${${_budget}} GREATER 4096)
        message(FATAL_ERROR "${_budget} must be an integer from1 to4096")
    endif()
endforeach()
if(AXIOM_RENDER_CACHE_MIB GREATER AXIOM_RENDER_SHARED_MIB)
    message(FATAL_ERROR "Optional render capacity cannot exceed the shared hard cap")
endif()
target_compile_definitions(canvas_runtime_render PRIVATE
    AXIOM_RENDER_CACHE_MIB=${AXIOM_RENDER_CACHE_MIB}
    AXIOM_RENDER_SHARED_MIB=${AXIOM_RENDER_SHARED_MIB})

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
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(CANVAS_CHALK_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(CANVAS_COMPACT_MATERIAL_HEADERS)
    foreach(material coverage material)
      if(material STREQUAL "coverage")
        set(material_symbol kInkCoverageSksl)
      else()
        set(material_symbol kInkMaterialSksl)
      endif()
      add_custom_command(
        OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/ink_${material}_sksl.hpp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${CANVAS_CHALK_GENERATED_DIR}/ink_${material}_sksl.hpp" "${material_symbol}"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v1/${material}.sksl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v1/${material}.sksl")
      list(APPEND CANVAS_COMPACT_MATERIAL_HEADERS "${CANVAS_CHALK_GENERATED_DIR}/ink_${material}_sksl.hpp")
    endforeach()
    foreach(part coverage material)
      add_custom_command(OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/crayon_${part}_v3_sksl.hpp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${CANVAS_CHALK_GENERATED_DIR}/crayon_${part}_v3_sksl.hpp" "kCrayon_${part}_v3"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/crayon-material-v3/${part}.sksl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/crayon-material-v3/${part}.sksl")
      list(APPEND CANVAS_COMPACT_MATERIAL_HEADERS "${CANVAS_CHALK_GENERATED_DIR}/crayon_${part}_v3_sksl.hpp")
    endforeach()
    foreach(part coverage material)
      add_custom_command(OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/drywet_${part}_v3_sksl.hpp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${CANVAS_CHALK_GENERATED_DIR}/drywet_${part}_v3_sksl.hpp" "kDryWet_${part}_v3"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-watercolor-v3/${part}.sksl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
                "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-watercolor-v3/${part}.sksl")
      list(APPEND CANVAS_COMPACT_MATERIAL_HEADERS "${CANVAS_CHALK_GENERATED_DIR}/drywet_${part}_v3_sksl.hpp")
    endforeach()
    add_custom_target(canvas_compact_material_assets DEPENDS ${CANVAS_COMPACT_MATERIAL_HEADERS})
    add_custom_command(
      OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/ink_material_v2_sksl.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/ink_material_v2_sksl.hpp" "kInkMaterialV2Sksl"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v2/material.sksl"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v2/material.sksl")
    add_custom_target(canvas_compact_material_v2_assets DEPENDS "${CANVAS_CHALK_GENERATED_DIR}/ink_material_v2_sksl.hpp")
    add_dependencies(canvas_runtime_render canvas_compact_material_v2_assets)
    add_custom_command(OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/ink_coverage_v3_sksl.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/ink_coverage_v3_sksl.hpp" kInkCoverageV3Sksl
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v3/coverage.sksl"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/compact-material-v3/coverage.sksl")
    add_custom_target(canvas_compact_coverage_v3_assets DEPENDS "${CANVAS_CHALK_GENERATED_DIR}/ink_coverage_v3_sksl.hpp")
    add_dependencies(canvas_runtime_render canvas_compact_coverage_v3_assets)
    add_dependencies(canvas_runtime_render canvas_compact_material_assets)
    add_custom_command(
      OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/chalk_shape_png.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/chalk_shape_png.hpp"
              "kCanvasChalkShapePng"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-grain-v1/shape-screenshot-extract-v2.png"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-grain-v1/shape-screenshot-extract-v2.png")
    add_custom_command(
      OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/chalk_grain_png.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/chalk_grain_png.hpp"
              "kCanvasChalkGrainPng"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-grain-v1/grain-screenshot-extract-v2.png"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/chalk-grain-v1/grain-screenshot-extract-v2.png")
    add_custom_command(
      OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/membrane_shape_png.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/membrane_shape_png.hpp"
              "kCanvasMembraneShapePng"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/membrane-v1/shape.png"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/membrane-v1/shape.png")
    add_custom_command(
      OUTPUT "${CANVAS_CHALK_GENERATED_DIR}/membrane_grain_png.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${CANVAS_CHALK_GENERATED_DIR}/membrane_grain_png.hpp"
              "kCanvasMembraneGrainPng"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/membrane-v1/grain.png"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
              "${PROJECT_SOURCE_DIR}/runtime/ink/assets/membrane-v1/grain.png")
    add_custom_target(canvas_chalk_texture_assets DEPENDS
      "${CANVAS_CHALK_GENERATED_DIR}/chalk_shape_png.hpp"
      "${CANVAS_CHALK_GENERATED_DIR}/chalk_grain_png.hpp"
      "${CANVAS_CHALK_GENERATED_DIR}/membrane_shape_png.hpp"
      "${CANVAS_CHALK_GENERATED_DIR}/membrane_grain_png.hpp")
    add_dependencies(canvas_runtime_render canvas_chalk_texture_assets)
    target_include_directories(canvas_runtime_render PRIVATE "${CANVAS_CHALK_GENERATED_DIR}")
    target_sources(canvas_runtime_render PRIVATE src/skia_headless_backend.cpp)
    target_sources(canvas_runtime_render PRIVATE src/image_resource.cpp src/image_encode.cpp)
    # Accepted Skia binaries omit RTTI; this private SkWStream subclass must match.
    if(MSVC)
      set_property(SOURCE src/image_encode.cpp APPEND PROPERTY COMPILE_OPTIONS /GR-)
    else()
      set_property(SOURCE src/image_encode.cpp APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)
    endif()
    target_sources(canvas_runtime_render PRIVATE src/skia_text_resources.cpp)
    target_sources(canvas_runtime_render PRIVATE src/brush_outline_skia_path.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_scene_renderer.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_compact_material.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_ink_backend.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_surface_provider.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_renderer.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_render_cache.cpp src/skia_navigation_tiles.cpp src/skia_prepared_geometry.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_brush_renderer.cpp)
    target_link_libraries(canvas_runtime_render PRIVATE CanvasSkia::Skia CanvasSkia::Paragraph)
    target_compile_definitions(canvas_runtime_render PUBLIC CANVAS_RENDER_HAS_SKIA=1)
endif()
target_include_directories(canvas_runtime_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/runtime/scene/include
    ${PROJECT_SOURCE_DIR}/runtime/text/include)
target_link_libraries(canvas_runtime_render PUBLIC
    canvas_runtime_foundation canvas_runtime_semantic canvas_runtime_scene
    canvas_runtime_ink canvas_runtime_input)
target_compile_features(canvas_runtime_render PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(canvas_runtime_render PRIVATE /W4 /permissive- /WX /EHsc)
    target_compile_options(canvas_runtime_render PRIVATE /wd4702)
else()
    target_compile_options(canvas_runtime_render PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(EMSCRIPTEN)
        include(CheckCXXCompilerFlag)
        include(CMakePushCheckState)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_FLAGS "-Werror")
        check_cxx_compiler_flag("-Wno-missing-designated-field-initializers"
            AXIOM_HAVE_MISSING_DESIGNATED_FIELD_SUPPRESSION)
        cmake_pop_check_state()
        if(AXIOM_HAVE_MISSING_DESIGNATED_FIELD_SUPPRESSION)
            target_compile_options(canvas_runtime_render PRIVATE
                -Wno-missing-designated-field-initializers)
        endif()
    endif()
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_erase_hit_tests tests/erase_hit_test.cpp)
    target_link_libraries(axiom_erase_hit_tests PRIVATE canvas_runtime_render)
    add_test(NAME G5EraseExactHit COMMAND axiom_erase_hit_tests)
    add_executable(axiom_render_cache_tests tests/render_cache_test.cpp)
    target_link_libraries(axiom_render_cache_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_render_cache_tests PRIVATE "${CANVAS_SKIA_SDK_ROOT}")
    add_test(NAME G5RenderCache COMMAND axiom_render_cache_tests)
    add_test(NAME G5MaskedCanonicalRecovery COMMAND axiom_render_cache_tests --masked-canonical-only)
    add_test(NAME G5ErasedMixedNavigation COMMAND axiom_render_cache_tests --masked-mixed-only)
    add_test(NAME G5RetainedZoomReentry COMMAND axiom_render_cache_tests --reentry-only)
    add_test(NAME G5MixedNavigationReentry COMMAND axiom_render_cache_tests --mixed-reentry-only)
    add_executable(axiom_tile_work_tests tests/tile_work_test.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(axiom_tile_work_tests PRIVATE canvas_runtime_render Threads::Threads)
    target_compile_features(axiom_tile_work_tests PRIVATE cxx_std_20)
    add_test(NAME G5TileWork COMMAND axiom_tile_work_tests)
    add_executable(axiom_render_damage_tests tests/damage_ownership_test.cpp)
    target_link_libraries(axiom_render_damage_tests PRIVATE canvas_runtime_render)
    add_test(NAME G5RenderDamage COMMAND axiom_render_damage_tests)
    add_executable(axiom_image_resource_tests tests/image_resource_test.cpp)
    target_link_libraries(axiom_image_resource_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_image_resource_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_image_resource_tests PRIVATE
      G47_IMAGE_FIXTURE="${PROJECT_SOURCE_DIR}/verification/fixtures/g47/image-stripes.png")
    if(MSVC)
      target_compile_options(axiom_image_resource_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_image_resource_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME G47ImageResource COMMAND axiom_image_resource_tests)
    add_executable(axiom_text_pipeline_tests tests/text_pipeline_test.cpp)
    target_link_libraries(axiom_text_pipeline_tests PRIVATE canvas_runtime_render)
    target_include_directories(axiom_text_pipeline_tests PRIVATE ${PROJECT_SOURCE_DIR}/runtime/scene/tests)
    target_compile_definitions(axiom_text_pipeline_tests PRIVATE G47_LATIN_FONT="${CanvasSkia_FONT_PATH}")
    if(MSVC)
      target_compile_options(axiom_text_pipeline_tests PRIVATE /UNDEBUG)
    else()
      target_compile_options(axiom_text_pipeline_tests PRIVATE -UNDEBUG)
    endif()
    if(WIN32 AND CanvasSkia_ICU_DATA_PATH)
      add_custom_command(TARGET axiom_text_pipeline_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CanvasSkia_ICU_DATA_PATH}"
          "$<TARGET_FILE_DIR:axiom_text_pipeline_tests>/icudtl.dat")
    endif()
    add_test(NAME G47TextPipeline COMMAND axiom_text_pipeline_tests)

    add_executable(axiom_render_brush_outline_skia_path_tests
        tests/brush_outline_skia_path_test.cpp)
    # This test consumes SkPath directly: use the locked SDK imported target,
    # as production does, instead of treating SDK headers as project headers.
    target_link_libraries(axiom_render_brush_outline_skia_path_tests
        PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_compile_definitions(axiom_render_brush_outline_skia_path_tests
        PRIVATE G45_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    target_compile_features(axiom_render_brush_outline_skia_path_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_brush_outline_skia_path_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_brush_outline_skia_path_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G45BrushOutlineSkiaPath
        COMMAND axiom_render_brush_outline_skia_path_tests)

    add_executable(axiom_render_skia_ink_backend_tests
        tests/skia_ink_backend_test.cpp)
    target_link_libraries(axiom_render_skia_ink_backend_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_skia_ink_backend_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_skia_ink_backend_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(axiom_render_skia_ink_backend_tests
                PRIVATE /clang:-Wno-deprecated-declarations)
        else()
            target_compile_options(axiom_render_skia_ink_backend_tests PRIVATE /wd4996)
        endif()
    else()
      target_compile_options(axiom_render_skia_ink_backend_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-deprecated-declarations)
    endif()
    add_test(NAME G4WindowsSkiaInkViewport
        COMMAND axiom_render_skia_ink_backend_tests)
    add_executable(axiom_render_skia_brush_primitive_tests
        tests/skia_brush_primitive_test.cpp)
    target_link_libraries(axiom_render_skia_brush_primitive_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_skia_brush_primitive_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_skia_brush_primitive_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(axiom_render_skia_brush_primitive_tests
                PRIVATE /clang:-Wno-deprecated-declarations)
        else()
            target_compile_options(axiom_render_skia_brush_primitive_tests PRIVATE /wd4996)
        endif()
    else()
        target_compile_options(axiom_render_skia_brush_primitive_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-deprecated-declarations)
    endif()
    add_test(NAME G4SkiaBrushPrimitiveDab COMMAND axiom_render_skia_brush_primitive_tests)
    add_executable(axiom_render_chalk_texture_tests
        tests/chalk_texture_test.cpp)
    target_link_libraries(axiom_render_chalk_texture_tests PRIVATE canvas_runtime_render)
    target_include_directories(axiom_render_chalk_texture_tests PRIVATE
      "${CANVAS_SKIA_SDK_ROOT}")
    target_compile_features(axiom_render_chalk_texture_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_render_chalk_texture_tests
        PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG /wd4267 /wd4201)
    else()
      target_compile_options(axiom_render_chalk_texture_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
      target_compile_options(axiom_render_chalk_texture_tests PRIVATE
        -Wno-error=gnu-zero-variadic-macro-arguments -Wno-error=nested-anon-types
        -Wno-error=gnu-anonymous-struct)
    endif()
    add_test(NAME G4SkiaChalkTexture COMMAND axiom_render_chalk_texture_tests)
    add_executable(axiom_render_chalk_continuous_texture_tests
        tests/chalk_continuous_texture_test.cpp)
    target_link_libraries(axiom_render_chalk_continuous_texture_tests PRIVATE canvas_runtime_render)
    target_include_directories(axiom_render_chalk_continuous_texture_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/runtime/render/include
        "${CANVAS_SKIA_SDK_ROOT}")
    target_compile_features(axiom_render_chalk_continuous_texture_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_render_chalk_continuous_texture_tests
        PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG /wd4267 /wd4201)
    else()
      target_compile_options(axiom_render_chalk_continuous_texture_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wno-error=gnu-zero-variadic-macro-arguments -Wno-error=nested-anon-types
        -Wno-error=gnu-anonymous-struct)
    endif()
    add_test(NAME G4SkiaChalkContinuousTexture COMMAND axiom_render_chalk_continuous_texture_tests)
    add_executable(axiom_ink_v2_work_tests tests/ink_v2_work_test.cpp)
    target_link_libraries(axiom_ink_v2_work_tests PRIVATE canvas_runtime_render)
    add_test(NAME InkV2Work COMMAND axiom_ink_v2_work_tests)
    add_test(NAME InkV2RetinaPreview COMMAND axiom_ink_v2_work_tests retina)
    add_executable(axiom_ink_v2_material_tests tests/ink_v2_material_test.cpp)
    target_link_libraries(axiom_ink_v2_material_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_ink_v2_material_tests PRIVATE "${CANVAS_SKIA_SDK_ROOT}")
    foreach(material marker chalk crayon water parity presets sampling cache scene-mask visual tuning quality wax wax-mask drywet drywet-parity drywet-mask chunk-range)
      add_test(NAME InkV2Material-${material} COMMAND axiom_ink_v2_material_tests ${material})
    endforeach()
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_runtime_app_binding_tests
        tests/runtime_app_binding_test.cpp)
    target_link_libraries(axiom_runtime_app_binding_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_runtime_app_binding_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_runtime_app_binding_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_runtime_app_binding_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G3RuntimeAppBinding COMMAND axiom_runtime_app_binding_tests)

    add_executable(axiom_runtime_isolation_tests
        tests/runtime_isolation_test.cpp)
    target_link_libraries(axiom_runtime_isolation_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_runtime_isolation_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_runtime_isolation_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_runtime_isolation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME FC00XRuntimeIsolation COMMAND axiom_runtime_isolation_tests)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_render_headless_canvas_demo_tests
        tests/headless_canvas_demo_test.cpp)
    target_link_libraries(axiom_render_headless_canvas_demo_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_headless_canvas_demo_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_render_headless_canvas_demo_tests PRIVATE
        G3_07_DEMO_BINARY="$<TARGET_FILE:axiom_headless_canvas_demo>"
        G3_07_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(axiom_render_headless_canvas_demo_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_headless_canvas_demo_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
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
            PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG
                -Wno-missing-field-initializers)
    endif()
    add_test(NAME G3HeadlessSkia COMMAND axiom_render_headless_skia_tests)
    add_test(NAME FC05ConnectorPendingRaster COMMAND axiom_render_headless_skia_tests connector-pending)
    add_test(NAME FC05StickyFractionalProduction COMMAND axiom_render_headless_skia_tests production-sticky-fractional)
    add_test(NAME FC01TriangleConnectorRaster COMMAND axiom_render_headless_skia_tests fc01)

    add_executable(axiom_render_skia_chunk_range_entry_tests
        tests/skia_chunk_range_entry_test.cpp)
    target_link_libraries(axiom_render_skia_chunk_range_entry_tests PRIVATE
        canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_render_skia_chunk_range_entry_tests PRIVATE
        "${CANVAS_SKIA_SDK_ROOT}")
    target_compile_features(axiom_render_skia_chunk_range_entry_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_skia_chunk_range_entry_tests
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_skia_chunk_range_entry_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG
            -Wno-error=gnu-zero-variadic-macro-arguments -Wno-error=nested-anon-types
            -Wno-error=gnu-anonymous-struct)
    endif()
    add_test(NAME G5SkiaChunkRangeEntry
        COMMAND axiom_render_skia_chunk_range_entry_tests)
    add_executable(axiom_prepared_geometry_tests tests/prepared_geometry_test.cpp)
    target_link_libraries(axiom_prepared_geometry_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_prepared_geometry_tests PRIVATE "${CANVAS_SKIA_SDK_ROOT}")
    target_compile_features(axiom_prepared_geometry_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_prepared_geometry_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG /GR-)
    else()
      # Published Skia omits RTTI for SkNoDrawCanvas. The recording oracle
      # derives from that SDK type and does not use RTTI itself.
      target_compile_options(axiom_prepared_geometry_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG -fno-rtti
        -Wno-missing-field-initializers -Wno-error=gnu-zero-variadic-macro-arguments
        -Wno-error=nested-anon-types -Wno-error=gnu-anonymous-struct)
    endif()
    add_test(NAME G5PreparedGeometry COMMAND axiom_prepared_geometry_tests)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(axiom_runtime_frame_coordinator_tests
        tests/runtime_frame_coordinator_test.cpp)
    target_link_libraries(axiom_runtime_frame_coordinator_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_runtime_frame_coordinator_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_runtime_frame_coordinator_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_runtime_frame_coordinator_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME FC00XFrameCoordinator COMMAND axiom_runtime_frame_coordinator_tests)

    add_executable(axiom_render_target_tests tests/render_target_test.cpp)
    target_link_libraries(axiom_render_target_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_target_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_target_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_target_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G3RenderTargetSwitching COMMAND axiom_render_target_tests)

    # These tests exercise the Skia-only providers below.  Keep them out of
    # non-Skia host configurations (such as POC-03/RF-01), where the provider
    # symbols are intentionally not compiled into canvas_runtime_render.
    if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
      add_executable(axiom_render_skia_surface_provider_tests
          tests/skia_surface_provider_test.cpp)
      target_link_libraries(axiom_render_skia_surface_provider_tests
          PRIVATE canvas_runtime_render)
      target_compile_features(axiom_render_skia_surface_provider_tests PRIVATE cxx_std_20)
      if(MSVC)
        target_compile_options(axiom_render_skia_surface_provider_tests
          PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
      else()
        target_compile_options(axiom_render_skia_surface_provider_tests
          PRIVATE -Wall -Wextra -Wpedantic -Werror)
      endif()
      add_test(NAME G3SkiaSurfaceProvider COMMAND axiom_render_skia_surface_provider_tests)

      add_executable(axiom_render_preview_surface_tests
          tests/preview_surface_test.cpp)
      target_link_libraries(axiom_render_preview_surface_tests
          PRIVATE canvas_runtime_render)
      target_compile_features(axiom_render_preview_surface_tests PRIVATE cxx_std_20)
      if(MSVC)
        target_compile_options(axiom_render_preview_surface_tests
          PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
      else()
        target_compile_options(axiom_render_preview_surface_tests
          PRIVATE -Wall -Wextra -Wpedantic -Werror)
      endif()
      add_test(NAME G3PreviewSurface COMMAND axiom_render_preview_surface_tests)
    endif()

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
            PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_frame_state_tests
            PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()

    add_test(NAME G3FrameState COMMAND axiom_render_frame_state_tests)

    add_executable(axiom_render_editing_overlay_tests
        tests/editing_overlay_test.cpp)
    target_link_libraries(axiom_render_editing_overlay_tests PRIVATE canvas_runtime_render)
    target_compile_features(axiom_render_editing_overlay_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_render_editing_overlay_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_render_editing_overlay_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G46EditingOverlay COMMAND axiom_render_editing_overlay_tests)

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
            PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
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
            PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
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

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
  add_executable(axiom_shared_scene_tests tests/shared_scene_test.cpp)
  target_link_libraries(axiom_shared_scene_tests PRIVATE canvas_runtime_render)
  if(TARGET CanvasSkia::Skia)
    target_link_libraries(axiom_shared_scene_tests PRIVATE CanvasSkia::Skia)
  endif()
  target_include_directories(axiom_shared_scene_tests PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
  target_compile_features(axiom_shared_scene_tests PRIVATE cxx_std_20)
  add_test(NAME G5SharedScenePayload COMMAND axiom_shared_scene_tests --payload)
  add_test(NAME G5SharedSceneSnapshot COMMAND axiom_shared_scene_tests --snapshot)
  add_test(NAME G5SharedSceneIndex COMMAND axiom_shared_scene_tests --index)
  add_test(NAME G5SharedSceneReaders COMMAND axiom_shared_scene_tests --readers)
  if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_test(NAME G5SharedSceneRetention COMMAND axiom_shared_scene_tests --retention)
    add_test(NAME G5SharedSceneParity COMMAND axiom_shared_scene_tests --parity)
  endif()
endif()
