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

include("${PROJECT_SOURCE_DIR}/cmake/AxiomRenderMemoryBudget.cmake")
axiom_render_memory_compile_definitions(canvas_runtime_render)

if(EMSCRIPTEN AND (CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR
                   CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH))
    target_sources(canvas_runtime_render PRIVATE src/webgl_surface_backend.cpp)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND AXIOM_CPU_PENDING_ERASE)
 foreach(kind numeric protocol compatibility)
  add_executable(axiom_cpu_local_exact_${kind}_tests tests/cpu_local_exact_${kind}_test.cpp)
  target_link_libraries(axiom_cpu_local_exact_${kind}_tests PRIVATE canvas_runtime_render canvas_runtime_interaction CanvasSkia::Skia)
  target_include_directories(axiom_cpu_local_exact_${kind}_tests PRIVATE "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include")
 endforeach()
 add_test(NAME G5CpuLocalExactNumeric COMMAND axiom_cpu_local_exact_numeric_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-local-numeric.json")
 add_test(NAME G5CpuLocalExactCornerContact COMMAND axiom_cpu_local_exact_numeric_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-local-corner-contact.json" --corner-contact)
 add_test(NAME G5CpuLocalExactIdentity COMMAND axiom_cpu_local_exact_protocol_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-local-identity.json")
 add_test(NAME G5CpuLocalExactCompatibility COMMAND axiom_cpu_local_exact_compatibility_tests)
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
    target_sources(canvas_runtime_render PRIVATE src/skia_compact_material.cpp src/render_execution.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_ink_backend.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_surface_provider.cpp)
    target_sources(canvas_runtime_render PRIVATE src/cpu_exact_path_kernel.cpp src/cpu_local_exact.cpp)
    target_sources(canvas_runtime_render PRIVATE src/cpu_worker_protocol.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_renderer.cpp)
    target_sources(canvas_runtime_render PRIVATE src/skia_render_cache.cpp src/skia_ink_layers.cpp src/skia_mask_history.cpp src/skia_navigation_tiles.cpp src/skia_prepared_geometry.cpp
        src/prepared_body_job.cpp src/navigation_pixel_validity.cpp)
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
    add_executable(axiom_ur3_webgl_extensions_tests tests/ur3_webgl_extensions_test.cpp)
    target_compile_features(axiom_ur3_webgl_extensions_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_ur3_webgl_extensions_tests PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(axiom_ur3_webgl_extensions_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME UR3WebGlExtensionRenewal COMMAND axiom_ur3_webgl_extensions_tests)
    add_executable(axiom_gpu_retirement_contract_tests tests/gpu_retirement_contract_test.cpp)
    target_include_directories(axiom_gpu_retirement_contract_tests PRIVATE src)
    target_link_libraries(axiom_gpu_retirement_contract_tests PRIVATE canvas_runtime_render)
    add_test(NAME G5GpuRetirementContract COMMAND axiom_gpu_retirement_contract_tests)
    add_executable(axiom_gpu_resource_recovery_tests tests/gpu_resource_recovery_test.cpp)
    target_link_libraries(axiom_gpu_resource_recovery_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    if(APPLE)
        target_link_libraries(axiom_gpu_resource_recovery_tests PRIVATE "-framework Foundation" objc)
    endif()
    add_test(NAME G5GpuResourceRecovery COMMAND axiom_gpu_resource_recovery_tests)
    add_executable(axiom_canonical_prepared_body_tests tests/canonical_prepared_body_test.cpp)
    target_link_libraries(axiom_canonical_prepared_body_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    if(APPLE)
        target_link_libraries(axiom_canonical_prepared_body_tests PRIVATE "-framework Foundation" objc)
    endif()
    add_test(NAME G5CanonicalPreparedBody COMMAND axiom_canonical_prepared_body_tests)
    add_executable(axiom_cpu_postworker_replay_tests tests/cpu_postworker_replay_test.cpp)
    target_link_libraries(axiom_cpu_postworker_replay_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_cpu_postworker_replay_tests PRIVATE "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include")
    add_test(NAME G5CpuPostworkerReplayReuse COMMAND axiom_cpu_postworker_replay_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-postworker-replay.json")
    add_test(NAME G5CanonicalPreparedBodyBounded COMMAND axiom_canonical_prepared_body_tests --bounded)
    if(APPLE)
        add_test(NAME G5CanonicalPreparedBodyPixels COMMAND axiom_canonical_prepared_body_tests --pixels)
        add_test(NAME G5CanonicalPreparedWorkingSet COMMAND axiom_canonical_prepared_body_tests --working-set)
    endif()
    add_executable(axiom_prepared_body_index_tests tests/prepared_body_index_test.cpp)
    target_link_libraries(axiom_prepared_body_index_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    if(APPLE)
        target_link_libraries(axiom_prepared_body_index_tests PRIVATE "-framework Foundation" objc)
    endif()
    add_test(NAME G5PreparedBodyIndex COMMAND axiom_prepared_body_index_tests)
    add_executable(axiom_render_execution_tests tests/render_execution_test.cpp)
    target_link_libraries(axiom_render_execution_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5RenderExecution COMMAND axiom_render_execution_tests)
    add_executable(axiom_erase_hit_tests tests/erase_hit_test.cpp)
    target_link_libraries(axiom_erase_hit_tests PRIVATE canvas_runtime_render)
    add_test(NAME G5EraseExactHit COMMAND axiom_erase_hit_tests)
    add_executable(axiom_render_cache_tests tests/render_cache_test.cpp)
    target_link_libraries(axiom_render_cache_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_render_cache_tests PRIVATE "${CANVAS_SKIA_SDK_ROOT}")
    add_test(NAME G5RenderCache COMMAND axiom_render_cache_tests)
    add_test(NAME G5CanonicalPreparedCompatibility COMMAND axiom_render_cache_tests --exact-pressure-only)
    add_test(NAME G5LiveEraseGpuMiss COMMAND axiom_render_cache_tests --live-erase-gpu-miss-only)
    add_test(NAME G5LiveEraseDpr COMMAND axiom_render_cache_tests --live-dpr-only)
    add_test(NAME G5TranslatedV4Drag COMMAND axiom_render_cache_tests --translated-v4-only)
    add_test(NAME G5InkBodyMaskReuse COMMAND axiom_render_cache_tests --body-mask-only)
    add_test(NAME G5RegionalMaskValidity COMMAND axiom_render_cache_tests --regional-mask-only)
    add_test(NAME G5MaskedNavigationProgress COMMAND axiom_render_cache_tests --masked-progress-only)
    add_test(NAME G5V4CoverageFallback COMMAND axiom_render_cache_tests --v4-fallback-only)
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
    add_executable(axiom_object_interaction_continuity_r2_tests
        tests/object_interaction_continuity_r2_test.cpp)
    target_link_libraries(axiom_object_interaction_continuity_r2_tests PRIVATE canvas_runtime_render)
    add_test(NAME G5ObjectInteractionLayoutIdentity
        COMMAND axiom_object_interaction_continuity_r2_tests)
    add_executable(axiom_ur2d_selection_up_tests tests/ur2d_selection_up_test.cpp)
    target_link_libraries(axiom_ur2d_selection_up_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5UR2DSelectionLocality COMMAND axiom_ur2d_selection_up_tests --candidate "${CMAKE_CURRENT_BINARY_DIR}/ur2d-selection")
    add_executable(axiom_erase_exact_canonical_tests tests/erase_exact_canonical_test.cpp)
    target_link_libraries(axiom_erase_exact_canonical_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5EraseExactCanonical COMMAND axiom_erase_exact_canonical_tests --verify "${CMAKE_CURRENT_BINARY_DIR}/erase-exact-canonical")
    add_executable(axiom_exact_query_working_set_tests tests/exact_query_working_set_test.cpp)
    add_executable(axiom_long_query_preparation_tests tests/long_query_preparation_test.cpp)
    add_executable(axiom_cpu_exact_kernel_tests tests/cpu_exact_kernel_test.cpp)
    add_executable(axiom_cpu_worker_protocol_tests tests/cpu_worker_protocol_test.cpp)
    add_executable(axiom_cpu_worker_behavior_red tests/cpu_worker_behavior_red.cpp)
    foreach(cpu_test axiom_cpu_exact_kernel_tests axiom_cpu_worker_protocol_tests axiom_cpu_worker_behavior_red)
      target_link_libraries(${cpu_test} PRIVATE canvas_runtime_render CanvasSkia::Skia)
      target_compile_features(${cpu_test} PRIVATE cxx_std_20)
    endforeach()
    add_test(NAME G5CpuExactKernel COMMAND axiom_cpu_exact_kernel_tests)
    add_test(NAME G5CpuWorkerProtocol COMMAND axiom_cpu_worker_protocol_tests)
    target_link_libraries(axiom_long_query_preparation_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_compile_definitions(axiom_long_query_preparation_tests PRIVATE AXIOM_UR2D_TESTS=1)
    target_compile_features(axiom_long_query_preparation_tests PRIVATE cxx_std_20)
    add_test(NAME G5LongQueryPreparationPath COMMAND axiom_long_query_preparation_tests
      "${CMAKE_CURRENT_BINARY_DIR}/long-query-preparation.json")
    add_executable(axiom_exact_history_path_residency_tests tests/exact_history_path_residency_test.cpp)
    target_link_libraries(axiom_exact_history_path_residency_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_compile_definitions(axiom_exact_history_path_residency_tests PRIVATE AXIOM_UR2D_TESTS=1)
    add_executable(axiom_exact_history_path_memory_tests tests/exact_history_path_memory_test.cpp)
    target_link_libraries(axiom_exact_history_path_memory_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_compile_definitions(axiom_exact_history_path_memory_tests PRIVATE AXIOM_UR2D_TESTS=1)
    add_test(NAME G5ExactHistoryPathResidency COMMAND axiom_exact_history_path_residency_tests
      "${PROJECT_SOURCE_DIR}/.aegis/results/G5-convergence-idle-up-r1/local-web/G5CV1-UP-NORMAL/long1600/final.axsnap"
      "${CMAKE_CURRENT_BINARY_DIR}/exact-history-path-residency.json" --verify)
    add_test(NAME G5ExactHistoryPathMemory COMMAND axiom_exact_history_path_memory_tests
      "${CMAKE_CURRENT_BINARY_DIR}/exact-history-path-memory.json")
    if(APPLE AND AXIOM_BUILD_RUNTIME_QUALIFICATION)
      add_executable(axiom_crossframe_history_runtime_tests tests/crossframe_history_runtime_test.cpp)
      target_link_libraries(axiom_crossframe_history_runtime_tests PRIVATE canvas_runtime_composition CanvasSkia::Skia "-framework Metal" "-framework Foundation")
      target_compile_definitions(axiom_crossframe_history_runtime_tests PRIVATE AXIOM_RUNTIME_QUALIFICATION=1 AXIOM_UR2D_TESTS=1)
      target_compile_features(axiom_crossframe_history_runtime_tests PRIVATE cxx_std_20)
      add_test(NAME G5CrossframeHistoryRuntime COMMAND axiom_crossframe_history_runtime_tests
        "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap"
        "${CMAKE_CURRENT_BINARY_DIR}/crossframe-history-runtime.json" 10)
      set_tests_properties(G5CrossframeHistoryRuntime PROPERTIES ENVIRONMENT "CFH_VERIFY=1" TIMEOUT 180)
      add_executable(axiom_long_query_preparation_runtime_tests tests/long_query_preparation_runtime_test.cpp)
      target_link_libraries(axiom_long_query_preparation_runtime_tests PRIVATE canvas_runtime_composition CanvasSkia::Skia "-framework Metal" "-framework Foundation")
      target_compile_definitions(axiom_long_query_preparation_runtime_tests PRIVATE AXIOM_RUNTIME_QUALIFICATION=1 AXIOM_UR2D_TESTS=1)
      target_compile_features(axiom_long_query_preparation_runtime_tests PRIVATE cxx_std_20)
      add_test(NAME G5LongQueryPreparationRuntime COMMAND axiom_long_query_preparation_runtime_tests
        "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap"
        "${CMAKE_CURRENT_BINARY_DIR}/long-query-preparation-runtime.json" 1)
      set_tests_properties(G5LongQueryPreparationRuntime PROPERTIES TIMEOUT 120)

    endif()
    add_executable(axiom_exact_owner_residency_tests tests/exact_owner_residency_test.cpp)
    add_executable(axiom_exact_owner_residency_pixels tests/exact_owner_residency_pixels.cpp)
    target_link_libraries(axiom_exact_owner_residency_pixels PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5ExactOwnerResidencyPixels COMMAND axiom_exact_owner_residency_pixels)
    target_compile_definitions(axiom_exact_owner_residency_tests PRIVATE AXIOM_EXACT_OWNER_RESIDENCY=1)
    target_link_libraries(axiom_exact_owner_residency_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5ExactOwnerResidency COMMAND axiom_exact_owner_residency_tests
      "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap"
      "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/final.axsnap"
      "${CMAKE_CURRENT_BINARY_DIR}/exact-owner-residency.json")
    target_compile_definitions(axiom_exact_query_working_set_tests PRIVATE AXIOM_EXACT_QUERY_WORKING_SET=1)
    target_link_libraries(axiom_exact_query_working_set_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    add_test(NAME G5ExactQueryWorkingSet COMMAND axiom_exact_query_working_set_tests --verify
      "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap"
      "${PROJECT_SOURCE_DIR}/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/final.axsnap"
      "${CMAKE_CURRENT_BINARY_DIR}/exact-query-working-set.json")
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

    if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
        add_executable(axiom_transient_reference_tests tests/transient_reference_test.cpp)
        target_link_libraries(axiom_transient_reference_tests PRIVATE
            canvas_runtime_render canvas_runtime_ink CanvasSkia::Skia)
        target_include_directories(axiom_transient_reference_tests PRIVATE
            ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
        target_compile_features(axiom_transient_reference_tests PRIVATE cxx_std_20)
        add_test(NAME G5TransientReference COMMAND axiom_transient_reference_tests)
    endif()

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

if(BUILD_TESTING AND AXIOM_BUILD_RUNTIME_QUALIFICATION AND NOT EMSCRIPTEN)
  if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(g5_transient_reference_component
      ${PROJECT_SOURCE_DIR}/verification/runners/g5/native/transient_reference.cpp)
    target_link_libraries(g5_transient_reference_component PRIVATE
      canvas_runtime_render canvas_runtime_ink)
    target_include_directories(g5_transient_reference_component PRIVATE
      ${PROJECT_SOURCE_DIR}/runtime/semantic/src
      ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
    target_compile_features(g5_transient_reference_component PRIVATE cxx_std_20)
    add_executable(axiom_ur0_fixture_oracle
      ${PROJECT_SOURCE_DIR}/verification/runners/g5/ur0/fixture_oracle.cpp
      ${PROJECT_SOURCE_DIR}/runtime/ink/src/compact_brush_session.cpp
      ${PROJECT_SOURCE_DIR}/runtime/ink/src/compact_brush_quality.cpp)
    target_link_libraries(axiom_ur0_fixture_oracle PRIVATE
      canvas_runtime_render canvas_runtime_ink CanvasSkia::Skia)
    target_include_directories(axiom_ur0_fixture_oracle PRIVATE
      ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
    target_compile_features(axiom_ur0_fixture_oracle PRIVATE cxx_std_20)
    # Match WASM scalar arithmetic for the independent knot replay. Native ARM
    # contraction otherwise changes a few coordinates by ~1e-13. This flag is
    # oracle-local; no production or third-party library is rebuilt/replaced.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(axiom_ur0_fixture_oracle PRIVATE -ffp-contract=off)
    endif()
  endif()
  add_executable(axiom_g5_observation_identity_tests tests/g5_observation_identity_test.cpp
    ${PROJECT_SOURCE_DIR}/runtime/foundation/src/g5_observation.cpp)
  target_compile_definitions(axiom_g5_observation_identity_tests PRIVATE AXIOM_G5_UR0_DIAGNOSTICS=1)
  target_include_directories(axiom_g5_observation_identity_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include ${PROJECT_SOURCE_DIR}/runtime/semantic/include
    ${PROJECT_SOURCE_DIR}/runtime/render/include ${PROJECT_SOURCE_DIR}/runtime/resources/include
    ${PROJECT_SOURCE_DIR}/runtime/scene/include ${PROJECT_SOURCE_DIR}/runtime/ink/include
    ${PROJECT_SOURCE_DIR}/runtime/input/include ${PROJECT_SOURCE_DIR}/runtime/text/include
    ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
  target_compile_features(axiom_g5_observation_identity_tests PRIVATE cxx_std_20)
  add_test(NAME G5Ur0ObservationIdentity COMMAND axiom_g5_observation_identity_tests)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
    add_executable(axiom_ur2a_current_composition_tests tests/ur2a_current_composition_test.cpp)
    target_link_libraries(axiom_ur2a_current_composition_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
    target_include_directories(axiom_ur2a_current_composition_tests PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
    target_compile_features(axiom_ur2a_current_composition_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_ur2a_current_composition_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(axiom_ur2a_current_composition_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME G5UR2ACompositionProof COMMAND axiom_ur2a_current_composition_tests)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND APPLE)
    add_executable(axiom_ur2a_mask_isolation_tests tests/ur2a_mask_isolation_test.cpp)
    target_link_libraries(axiom_ur2a_mask_isolation_tests PRIVATE canvas_runtime_render CanvasSkia::Skia "-framework Foundation" objc)
    target_compile_features(axiom_ur2a_mask_isolation_tests PRIVATE cxx_std_20)
    target_compile_options(axiom_ur2a_mask_isolation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    add_test(NAME G5UR2AMaskIsolationDirect COMMAND axiom_ur2a_mask_isolation_tests direct)
    add_test(NAME G5UR2AMaskIsolationPrepared COMMAND axiom_ur2a_mask_isolation_tests prepared)
    add_test(NAME G5UR2AMaskIsolationBackdrop COMMAND axiom_ur2a_mask_isolation_tests backdrop)
    add_test(NAME G5UR2AMaskIsolationLegacy COMMAND axiom_ur2a_mask_isolation_tests legacy)
endif()

if(BUILD_TESTING AND AXIOM_BUILD_RUNTIME_QUALIFICATION AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND NOT EMSCRIPTEN)
  add_executable(axiom_ur2b_exact_oracle ${PROJECT_SOURCE_DIR}/verification/runners/g5/ur2b_exact_oracle.cpp)
  target_link_libraries(axiom_ur2b_exact_oracle PRIVATE canvas_runtime_render canvas_runtime_ink CanvasSkia::Skia)
  target_include_directories(axiom_ur2b_exact_oracle PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
  target_compile_features(axiom_ur2b_exact_oracle PRIVATE cxx_std_20)
endif()

if(BUILD_TESTING AND AXIOM_BUILD_RUNTIME_QUALIFICATION AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND APPLE)
  add_executable(axiom_ur2b_exact_query_tests tests/ur2b_exact_query_test.cpp)
  target_link_libraries(axiom_ur2b_exact_query_tests PRIVATE canvas_runtime_render CanvasSkia::Skia "-framework Foundation" objc)
  target_include_directories(axiom_ur2b_exact_query_tests PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
  target_compile_features(axiom_ur2b_exact_query_tests PRIVATE cxx_std_20)
  foreach(kind Parity BodySharing Locality Invalidation Pressure)
    add_test(NAME G5UR2BExact${kind} COMMAND axiom_ur2b_exact_query_tests ${kind})
  endforeach()
endif()

if(TARGET axiom_ur2b_exact_oracle)
  add_executable(axiom_ur2b_exact_candidate ${PROJECT_SOURCE_DIR}/verification/runners/g5/ur2b_exact_oracle.cpp)
  target_link_libraries(axiom_ur2b_exact_candidate PRIVATE canvas_runtime_render canvas_runtime_ink CanvasSkia::Skia)
  target_include_directories(axiom_ur2b_exact_candidate PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
  target_compile_definitions(axiom_ur2b_exact_candidate PRIVATE UR2B_CANDIDATE_QUERY=1)
  target_compile_features(axiom_ur2b_exact_candidate PRIVATE cxx_std_20)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
 add_executable(axiom_ur2c_mask_history_tests tests/ur2c_mask_history_test.cpp)
 target_link_libraries(axiom_ur2c_mask_history_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 if(APPLE)
  target_link_libraries(axiom_ur2c_mask_history_tests PRIVATE "-framework Foundation" objc)
 endif()
 add_test(NAME G5UR2CHistoryPlanReuse COMMAND axiom_ur2c_mask_history_tests --reuse)
 add_test(NAME G5UR2CHistoryInvalidation COMMAND axiom_ur2c_mask_history_tests --invalidation)
 add_test(NAME G5UR2CHistoryExactParity COMMAND axiom_ur2c_mask_history_tests --exact)
 add_test(NAME G5UR2CLeaseAccounting COMMAND axiom_ur2c_mask_history_tests --lease)
 add_test(NAME G5UR2CHistoryProgress COMMAND axiom_ur2c_mask_history_tests --progress)
 add_test(NAME G5UR2CHistoryAdmission COMMAND axiom_ur2c_mask_history_tests --admission)
 add_test(NAME G5UR2CHistoryProgressSafety COMMAND axiom_ur2c_mask_history_tests --progress-safety)
endif()

# Private native fault injection is never compiled into ordinary Web consumers.
if(CANVAS_RENDER_BUILD_TESTS AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND NOT EMSCRIPTEN)
 add_executable(axiom_g5_convergence_context_tests tests/g5_convergence_context_test.cpp)
 target_link_libraries(axiom_g5_convergence_context_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 target_compile_definitions(axiom_g5_convergence_context_tests PRIVATE AXIOM_TRANSIENT_DISPLAY_TESTS=1)
 target_compile_features(axiom_g5_convergence_context_tests PRIVATE cxx_std_20)
 if(MSVC)
  target_compile_options(axiom_g5_convergence_context_tests PRIVATE /W4 /WX /UNDEBUG)
 else()
  target_compile_options(axiom_g5_convergence_context_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
 endif()
 add_test(NAME G5ConvergenceContextRelease COMMAND axiom_g5_convergence_context_tests)
 target_compile_definitions(canvas_runtime_render PRIVATE AXIOM_TRANSIENT_DISPLAY_TESTS=1)
 if(AXIOM_BUILD_RUNTIME_QUALIFICATION)
  target_compile_definitions(canvas_runtime_render PRIVATE AXIOM_UR2D_TESTS=1)
 endif()
 add_executable(axiom_transient_display_retry_tests tests/transient_display_retry_test.cpp)
 target_compile_definitions(axiom_transient_display_retry_tests PRIVATE AXIOM_TRANSIENT_DISPLAY_TESTS=1)
 target_compile_definitions(axiom_transient_display_retry_tests PRIVATE G47_LATIN_FONT="${CanvasSkia_FONT_PATH}")
 add_executable(axiom_render_memory_budget_tests tests/render_memory_budget_test.cpp)
 target_compile_definitions(axiom_render_memory_budget_tests PRIVATE AXIOM_TRANSIENT_DISPLAY_TESTS=1)
 axiom_render_memory_compile_definitions(axiom_render_memory_budget_tests)
 target_link_libraries(axiom_render_memory_budget_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 if(APPLE)
  target_link_libraries(axiom_render_memory_budget_tests PRIVATE "-framework Foundation" objc)
 endif()
 add_test(NAME G5RenderMemoryBudget COMMAND axiom_render_memory_budget_tests)
 target_link_libraries(axiom_transient_display_retry_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 if(APPLE)
  target_link_libraries(axiom_transient_display_retry_tests PRIVATE "-framework Foundation" objc)
 endif()
 add_test(NAME G5TransientDisplayRetryPixels COMMAND axiom_transient_display_retry_tests --pixels "${CMAKE_CURRENT_BINARY_DIR}/transient-display-pixels")
 add_test(NAME G5TransientDisplayContinuity COMMAND axiom_transient_display_retry_tests --continuity "${CMAKE_CURRENT_BINARY_DIR}/transient-display-continuity")
 add_test(NAME G5TransientDisplayRecovery COMMAND axiom_transient_display_retry_tests --recovery "${CMAKE_CURRENT_BINARY_DIR}/transient-display-recovery")
 add_test(NAME G5ObjectInteractionStaticOracle COMMAND axiom_transient_display_retry_tests --object-continuity "${CMAKE_CURRENT_BINARY_DIR}/object-continuity")
 add_test(NAME G5ObjectInteractionDenseMixed COMMAND axiom_transient_display_retry_tests --dense-objects "${CMAKE_CURRENT_BINARY_DIR}/object-continuity-dense")
 add_test(NAME G5DesktopRenderBudgetArea COMMAND axiom_transient_display_retry_tests --desktop-budget "${CMAKE_CURRENT_BINARY_DIR}/desktop-budget-area")
endif()

if(CANVAS_RENDER_BUILD_TESTS AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND NOT EMSCRIPTEN)
 add_test(NAME G5TransientDisplayHandoff COMMAND axiom_transient_display_retry_tests --handoff "${CMAKE_CURRENT_BINARY_DIR}/transient-display-handoff")
 add_test(NAME G5TransientDisplayHandoffInflight COMMAND axiom_transient_display_retry_tests --handoff-inflight "${CMAKE_CURRENT_BINARY_DIR}/transient-display-handoff-inflight")
 add_executable(axiom_transient_display_trajectory_oracle "${PROJECT_SOURCE_DIR}/verification/runners/g5/transient_display_trajectory_oracle.cpp")
 target_link_libraries(axiom_transient_display_trajectory_oracle PRIVATE canvas_runtime_render canvas_runtime_interaction CanvasSkia::Skia)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
 foreach(kind protocol long_path)
  add_executable(axiom_cpu_pending_${kind}_tests tests/cpu_pending_${kind}_test.cpp)
  target_link_libraries(axiom_cpu_pending_${kind}_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 endforeach()
 add_test(NAME G5CpuPendingProtocol COMMAND axiom_cpu_pending_protocol_tests)
 add_test(NAME G5CpuPendingLongPath COMMAND axiom_cpu_pending_long_path_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-pending7200.path")
 set_tests_properties(G5CpuPendingLongPath PROPERTIES TIMEOUT 120)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
 foreach(kind custody cache)
  add_executable(axiom_cpu_history_${kind}_tests tests/cpu_history_${kind}_test.cpp)
  target_link_libraries(axiom_cpu_history_${kind}_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
  target_compile_definitions(axiom_cpu_history_${kind}_tests PRIVATE AXIOM_UR2D_TESTS=1)
 endforeach()
 add_test(NAME G5CpuHistoryCustody COMMAND axiom_cpu_history_custody_tests "${CMAKE_CURRENT_BINARY_DIR}/cpu-history-custody.json")
 add_test(NAME G5CpuHistoryCache COMMAND axiom_cpu_history_cache_tests)
 set_tests_properties(G5CpuHistoryCustody G5CpuHistoryCache PROPERTIES TIMEOUT 120)
endif()

if(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS)
 add_executable(axiom_cpu_prefix_continuation_tests tests/cpu_prefix_continuation_test.cpp)
 target_link_libraries(axiom_cpu_prefix_continuation_tests PRIVATE canvas_runtime_render CanvasSkia::Skia)
 add_test(NAME G5CpuPrefixContinuation COMMAND axiom_cpu_prefix_continuation_tests)
 set_tests_properties(G5CpuPrefixContinuation PROPERTIES TIMEOUT 120)
endif()
