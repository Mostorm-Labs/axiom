add_library(canvas_runtime_scene STATIC
    src/damage_tracker.cpp
    src/direct_render_scene.cpp
    src/scene.cpp
    src/scene_binding.cpp
    src/incremental_runtime_coordinator.cpp
    src/incremental_runtime_full_materialization_bridge.cpp
    src/bounds_system.cpp
    src/hierarchy_geometry.cpp
    src/reference_geometry.cpp
    src/derived_geometry.cpp
    src/connector_router.cpp
    src/scene_impact.cpp
    src/scene_dependency_graph.cpp
    src/scene_record_store.cpp
    src/scene_delta.cpp
    src/scene_view.cpp
    src/shadow_render_scene.cpp
    src/spatial_delta.cpp
    src/linear_spatial_index.cpp
    src/uniform_grid_spatial_index.cpp)
target_sources(canvas_runtime_scene PRIVATE
    src/full_scene_compiler.cpp
    src/runtime_scene_projection.cpp)
target_include_directories(canvas_runtime_scene PUBLIC include
                           ${PROJECT_SOURCE_DIR}/runtime/semantic/include)
target_link_libraries(canvas_runtime_scene PUBLIC canvas_runtime_foundation
                      canvas_runtime_semantic canvas_runtime_text)
target_compile_features(canvas_runtime_scene PUBLIC cxx_std_20)
if(MSVC)
    set_property(SOURCE src/hierarchy_geometry.cpp APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    set_property(SOURCE src/hierarchy_geometry.cpp APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
endif()

add_executable(axiom_fc05_route_probe tools/fc05_route_probe.cpp)
target_link_libraries(axiom_fc05_route_probe PRIVATE canvas_runtime_scene)
if(MSVC)
    target_compile_options(axiom_fc05_route_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(axiom_fc05_route_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(EMSCRIPTEN)
    target_link_options(axiom_fc05_route_probe PRIVATE -sENVIRONMENT=node -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1)
endif()

if(MSVC)
    target_compile_options(canvas_runtime_scene PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_runtime_scene PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CANVAS_RF01_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(testing)
    add_subdirectory(tests)
endif()
