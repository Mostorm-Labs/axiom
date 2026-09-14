add_executable(canvas_rf01_scene_atomicity_test scene_atomicity_test.cpp)
target_link_libraries(canvas_rf01_scene_atomicity_test
    PRIVATE canvas_runtime_scene_fakes canvas_runtime_semantic)

if(MSVC)
    target_compile_options(canvas_rf01_scene_atomicity_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(
        canvas_rf01_scene_atomicity_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME canvas_rf01_scene_atomicity COMMAND canvas_rf01_scene_atomicity_test)

add_executable(canvas_rf01_full_rebuild_test scene_full_rebuild_test.cpp)
target_link_libraries(canvas_rf01_full_rebuild_test PRIVATE canvas_runtime_scene_fakes)

if(MSVC)
    target_compile_options(canvas_rf01_full_rebuild_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(
        canvas_rf01_full_rebuild_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME canvas_rf01_full_rebuild COMMAND canvas_rf01_full_rebuild_test)

add_executable(canvas_rf01_scene_binding_test scene_binding_test.cpp)
target_link_libraries(canvas_rf01_scene_binding_test PRIVATE canvas_runtime_scene_fakes)

if(MSVC)
    target_compile_options(canvas_rf01_scene_binding_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(
        canvas_rf01_scene_binding_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME canvas_rf01_scene_binding COMMAND canvas_rf01_scene_binding_test)

add_executable(canvas_g2_a0_semantic_generation_binding
    semantic_generation_binding_test.cpp)
target_link_libraries(canvas_g2_a0_semantic_generation_binding
    PRIVATE canvas_runtime_scene canvas_runtime_semantic canvas_runtime_scene_fakes)

if(MSVC)
    target_compile_options(
        canvas_g2_a0_semantic_generation_binding PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(
        canvas_g2_a0_semantic_generation_binding PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME canvas_g2_a0_semantic_generation_binding
         COMMAND canvas_g2_a0_semantic_generation_binding)

add_executable(canvas_g2_a1_runtime_record_projection
    runtime_record_projection_test.cpp)
target_link_libraries(canvas_g2_a1_runtime_record_projection
    PRIVATE canvas_runtime_scene canvas_runtime_semantic)
target_include_directories(canvas_g2_a1_runtime_record_projection
    PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
if(MSVC)
    target_compile_options(canvas_g2_a1_runtime_record_projection PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_g2_a1_runtime_record_projection PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME canvas_g2_a1_runtime_record_projection
         COMMAND canvas_g2_a1_runtime_record_projection)

add_executable(canvas_g2_a2_bounds_system_test bounds_system_test.cpp)
target_link_libraries(canvas_g2_a2_bounds_system_test PRIVATE canvas_runtime_scene)
target_compile_features(canvas_g2_a2_bounds_system_test PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a2_bounds_system COMMAND canvas_g2_a2_bounds_system_test)

add_executable(canvas_g2_a2_reference_geometry_test reference_geometry_test.cpp)
target_link_libraries(canvas_g2_a2_reference_geometry_test PRIVATE canvas_runtime_scene)
target_compile_features(canvas_g2_a2_reference_geometry_test PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a2_reference_geometry COMMAND canvas_g2_a2_reference_geometry_test)

add_executable(canvas_g2_a2_scene_impact_test scene_impact_test.cpp)
target_link_libraries(canvas_g2_a2_scene_impact_test PRIVATE canvas_runtime_scene)
target_compile_features(canvas_g2_a2_scene_impact_test PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a2_scene_impact COMMAND canvas_g2_a2_scene_impact_test)

add_executable(canvas_g2_a2_dependency_closure_test dependency_closure_test.cpp)
target_link_libraries(canvas_g2_a2_dependency_closure_test PRIVATE canvas_runtime_scene)
target_compile_features(canvas_g2_a2_dependency_closure_test PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a2_dependency_closure COMMAND canvas_g2_a2_dependency_closure_test)

add_executable(canvas_g2_a3_full_incremental_equivalence
    full_incremental_equivalence_test.cpp)
target_link_libraries(canvas_g2_a3_full_incremental_equivalence
    PRIVATE canvas_runtime_scene canvas_runtime_semantic)
target_include_directories(canvas_g2_a3_full_incremental_equivalence
    PRIVATE ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
target_compile_features(canvas_g2_a3_full_incremental_equivalence PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a3_full_incremental_equivalence
         COMMAND canvas_g2_a3_full_incremental_equivalence)

add_executable(canvas_g2_a4_scene_delta_test scene_delta_test.cpp)
target_link_libraries(canvas_g2_a4_scene_delta_test PRIVATE canvas_runtime_scene_fakes)
target_compile_features(canvas_g2_a4_scene_delta_test PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a4_scene_delta COMMAND canvas_g2_a4_scene_delta_test)

add_executable(canvas_g2_a6_incremental_equivalence
    incremental_runtime_coordinator_test.cpp
    incremental_runtime_full_oracle_adapter.cpp)
target_link_libraries(canvas_g2_a6_incremental_equivalence
    PRIVATE canvas_runtime_scene canvas_runtime_semantic canvas_runtime_scene_fakes)
target_compile_features(canvas_g2_a6_incremental_equivalence PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a6_incremental_equivalence
         COMMAND canvas_g2_a6_incremental_equivalence)

add_executable(canvas_g2_a6_atomic_publication incremental_runtime_atomic_publication_test.cpp)
target_link_libraries(canvas_g2_a6_atomic_publication
    PRIVATE canvas_runtime_scene canvas_runtime_semantic canvas_runtime_scene_fakes)
target_compile_features(canvas_g2_a6_atomic_publication PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a6_atomic_publication COMMAND canvas_g2_a6_atomic_publication)

add_executable(canvas_g2_a6_recovery incremental_runtime_recovery_test.cpp)
target_link_libraries(canvas_g2_a6_recovery
    PRIVATE canvas_runtime_scene canvas_runtime_semantic canvas_runtime_scene_fakes)
target_compile_features(canvas_g2_a6_recovery PRIVATE cxx_std_20)
add_test(NAME canvas_g2_a6_recovery COMMAND canvas_g2_a6_recovery)

# G2-A5 P36 uses a standalone executable so the frozen spatial oracle can be
# compiled and executed even when an unrelated Scene target fails earlier in
# the full repository build. The exact production spatial sources are compiled
# into this target; no mock implementation is substituted.
add_executable(canvas_g2_a5_spatial_index_differential_test
    spatial_index_differential_test.cpp
    ../src/linear_spatial_index.cpp
    ../src/uniform_grid_spatial_index.cpp)
target_include_directories(canvas_g2_a5_spatial_index_differential_test PRIVATE
    ${PROJECT_SOURCE_DIR}/runtime/scene/include
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include)
target_compile_features(canvas_g2_a5_spatial_index_differential_test PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(canvas_g2_a5_spatial_index_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_g2_a5_spatial_index_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME canvas_g2_a5_spatial_index_differential
         COMMAND canvas_g2_a5_spatial_index_differential_test)
add_custom_command(TARGET canvas_g2_a5_spatial_index_differential_test POST_BUILD
    COMMAND $<TARGET_FILE:canvas_g2_a5_spatial_index_differential_test>
    COMMENT "Run GT-G2-00-A5 P36 spatial differential oracle")
add_dependencies(canvas_runtime_scene canvas_g2_a5_spatial_index_differential_test)

add_executable(canvas_g2_a5_d6_spatial_index_benchmark
    spatial_index_d6_benchmark.cpp
    ../src/linear_spatial_index.cpp
    ../src/uniform_grid_spatial_index.cpp)
target_include_directories(canvas_g2_a5_d6_spatial_index_benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/runtime/scene/include
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include)
target_compile_features(canvas_g2_a5_d6_spatial_index_benchmark PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(canvas_g2_a5_d6_spatial_index_benchmark PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_g2_a5_d6_spatial_index_benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME canvas_g2_a5_d6_spatial_index_benchmark
         COMMAND canvas_g2_a5_d6_spatial_index_benchmark
                 --output ${CMAKE_CURRENT_BINARY_DIR}/GT-G2-00-A5-D6-Performance-Evidence-v0.1.json)

add_executable(canvas_rf01_damage_view_test damage_view_test.cpp)
target_link_libraries(canvas_rf01_damage_view_test PRIVATE canvas_runtime_scene)

if(MSVC)
    target_compile_options(canvas_rf01_damage_view_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(
        canvas_rf01_damage_view_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME canvas_rf01_damage_view COMMAND canvas_rf01_damage_view_test)

add_library(canvas_rf01_public_abi_c OBJECT public_abi_c_test.c)
target_include_directories(canvas_rf01_public_abi_c PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(canvas_rf01_public_abi_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

add_library(canvas_rf01_public_abi_cpp OBJECT public_abi_cpp_test.cpp)
target_include_directories(canvas_rf01_public_abi_cpp PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(canvas_rf01_public_abi_cpp PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(canvas_rf01_public_abi_c PRIVATE /W4 /WX)
    target_compile_options(canvas_rf01_public_abi_cpp PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_rf01_public_abi_c PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(
        canvas_rf01_public_abi_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

foreach(header IN ITEMS
        canvas/foundation/object_id.hpp
        canvas/foundation/result.hpp
        canvas/foundation/revision.hpp
        canvas/foundation/stable_order_key.hpp
        canvas/foundation/world_geometry.hpp
        canvas/scene/damage_tracker.hpp
        canvas/scene/direct_render_scene.hpp
        canvas/scene/render_scene.hpp
        canvas/scene/scene.hpp
        canvas/scene/scene_binding.hpp
        canvas/scene/scene_compiler.hpp
        canvas/scene/scene_frame.hpp
        canvas/scene/scene_query.hpp
        canvas/scene/scene_record_store.hpp
        canvas/scene/scene_types.hpp
        canvas/scene/scene_view.hpp
        canvas/scene/shadow_render_scene.hpp
        canvas/scene/spatial_index.hpp
        canvas/scene/spatial_delta.hpp
        canvas/scene/linear_spatial_index.hpp
        canvas/scene/uniform_grid_spatial_index.hpp)
    string(REPLACE "/" "_" target_suffix ${header})
    string(REPLACE "." "_" target_suffix ${target_suffix})
    set(source ${CMAKE_CURRENT_BINARY_DIR}/self_contained_${target_suffix}.cpp)
    file(WRITE ${source} "#include \"${header}\"\n")
    add_library(canvas_rf01_${target_suffix} OBJECT ${source})
    target_link_libraries(canvas_rf01_${target_suffix} PRIVATE canvas_runtime_scene)
    if(MSVC)
        target_compile_options(
            canvas_rf01_${target_suffix} PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(
            canvas_rf01_${target_suffix} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME canvas_rf01_module_boundaries
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_runtime_boundaries.py
            --root ${PROJECT_SOURCE_DIR})
add_test(
    NAME canvas_rf01_public_abi_manifest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_runtime_abi_manifest.py
            --root ${PROJECT_SOURCE_DIR})
