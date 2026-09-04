add_executable(canvas_semantic_types_test semantic_types_test.cpp)
target_link_libraries(canvas_semantic_types_test PRIVATE canvas_runtime_semantic GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_types_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_types_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(GoogleTest)
gtest_discover_tests(canvas_semantic_types_test)

add_executable(canvas_semantic_codec_test codec_test.cpp codec_negative_test.cpp)
target_link_libraries(canvas_semantic_codec_test PRIVATE canvas_runtime_semantic GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_codec_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_codec_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_codec_test)

add_executable(canvas_semantic_g1_06_snapshot_test
  g1_06_snapshot_test.cpp
  ../tools/g1_06_projection.cpp)
target_link_libraries(canvas_semantic_g1_06_snapshot_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_06_snapshot_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../tools
  ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_snapshot_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_snapshot_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_snapshot_test)

add_executable(canvas_semantic_g1_06_snapshot_bootstrap_test
  g1_06_snapshot_bootstrap_test.cpp
  ../tools/g1_06_projection.cpp)
target_link_libraries(canvas_semantic_g1_06_snapshot_bootstrap_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_06_snapshot_bootstrap_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../tools
  ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_snapshot_bootstrap_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_snapshot_bootstrap_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_snapshot_bootstrap_test TEST_PREFIX "g1_06_snapshot_bootstrap.")

add_executable(canvas_semantic_normalization_test normalization_test.cpp)
target_link_libraries(canvas_semantic_normalization_test PRIVATE canvas_runtime_semantic GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_normalization_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_normalization_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_normalization_test)

add_executable(canvas_semantic_envelope_validation_test envelope_validation_test.cpp)
target_link_libraries(canvas_semantic_envelope_validation_test PRIVATE canvas_runtime_semantic GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_envelope_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_envelope_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_envelope_validation_test)

# Compile the stateless validator directly for this contract test. This target
# intentionally does not link canvas_runtime_semantic, reference/indexed object
# stores, or mutation helpers.
add_executable(canvas_semantic_payload_structure_test
  payload_structure_test.cpp
  ../src/validator.cpp)
target_include_directories(canvas_semantic_payload_structure_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(canvas_semantic_payload_structure_test
  PRIVATE canvas_runtime_foundation GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_payload_structure_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_payload_structure_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_payload_structure_test)

add_executable(canvas_semantic_g1_04_c_geometry_overflow_source_test
  g1_04_c_geometry_overflow_source_test.cpp)
target_include_directories(canvas_semantic_g1_04_c_geometry_overflow_source_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(canvas_semantic_g1_04_c_geometry_overflow_source_test
  PRIVATE canvas_runtime_foundation GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_g1_04_c_geometry_overflow_source_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_04_c_geometry_overflow_source_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_g1_04_c_geometry_overflow_source_test)

add_executable(canvas_semantic_leaf_structure_validation_test
  leaf_structure_validation_test.cpp
  ../src/validator.cpp)
target_include_directories(canvas_semantic_leaf_structure_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(canvas_semantic_leaf_structure_validation_test
  PRIVATE canvas_runtime_foundation GTest::gtest_main)

if(MSVC)
  target_compile_options(canvas_semantic_leaf_structure_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_leaf_structure_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_leaf_structure_validation_test)

add_executable(canvas_semantic_a_lane_no_mutation_test
  a_lane_no_mutation_test.cpp)
target_link_libraries(canvas_semantic_a_lane_no_mutation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_a_lane_no_mutation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_a_lane_no_mutation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_a_lane_no_mutation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_a_lane_no_mutation_test)

add_executable(canvas_semantic_reference_object_store_test reference_object_store_test.cpp)
target_link_libraries(canvas_semantic_reference_object_store_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_reference_object_store_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_reference_object_store_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_reference_object_store_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_reference_object_store_test)

add_executable(canvas_semantic_indexed_object_store_test indexed_object_store_test.cpp)
target_link_libraries(canvas_semantic_indexed_object_store_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_indexed_object_store_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_indexed_object_store_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_indexed_object_store_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_indexed_object_store_test)

add_executable(canvas_semantic_object_store_differential_test object_store_differential_test.cpp)
target_link_libraries(canvas_semantic_object_store_differential_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_object_store_differential_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_object_store_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_object_store_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_object_store_differential_test)

add_executable(canvas_semantic_stateful_context_test stateful_context_test.cpp)
target_link_libraries(canvas_semantic_stateful_context_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_stateful_context_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_stateful_context_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_stateful_context_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_stateful_context_test)

add_executable(canvas_semantic_stateful_kind_applicability_test
  stateful_kind_applicability_test.cpp)
target_link_libraries(canvas_semantic_stateful_kind_applicability_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_stateful_kind_applicability_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_stateful_kind_applicability_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_stateful_kind_applicability_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_stateful_kind_applicability_test)

add_executable(canvas_semantic_staged_object_view_test staged_object_view_test.cpp)
target_link_libraries(canvas_semantic_staged_object_view_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_staged_object_view_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_staged_object_view_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_staged_object_view_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_staged_object_view_test)

add_executable(canvas_semantic_connector_stateful_validation_test
  connector_stateful_validation_test.cpp)
target_link_libraries(canvas_semantic_connector_stateful_validation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_connector_stateful_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_connector_stateful_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_connector_stateful_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_connector_stateful_validation_test)

add_executable(canvas_semantic_idempotency_test idempotency_test.cpp)
target_link_libraries(canvas_semantic_idempotency_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_idempotency_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(MSVC)
  target_compile_options(canvas_semantic_idempotency_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_idempotency_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

gtest_discover_tests(canvas_semantic_idempotency_test)

add_executable(canvas_semantic_hierarchy_stateful_validation_test hierarchy_stateful_validation_test.cpp)
target_link_libraries(canvas_semantic_hierarchy_stateful_validation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_hierarchy_stateful_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_hierarchy_stateful_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_hierarchy_stateful_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_hierarchy_stateful_validation_test)

add_executable(canvas_semantic_hierarchy_capability_validation_test
  hierarchy_capability_validation_test.cpp)
target_link_libraries(canvas_semantic_hierarchy_capability_validation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_hierarchy_capability_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_hierarchy_capability_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_hierarchy_capability_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_hierarchy_capability_validation_test)

add_executable(canvas_semantic_delete_closure_test delete_closure_test.cpp)
target_link_libraries(canvas_semantic_delete_closure_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_delete_closure_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_delete_closure_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_delete_closure_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_delete_closure_test)

add_executable(canvas_semantic_restore_stateful_validation_test
  restore_stateful_validation_test.cpp)
target_link_libraries(canvas_semantic_restore_stateful_validation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_restore_stateful_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_restore_stateful_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_restore_stateful_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_restore_stateful_validation_test)

add_executable(canvas_semantic_operation_specific_stateful_validation_test
  operation_specific_stateful_validation_test.cpp)
target_link_libraries(canvas_semantic_operation_specific_stateful_validation_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_specific_stateful_validation_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_specific_stateful_validation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_specific_stateful_validation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_specific_stateful_validation_test)

add_executable(canvas_semantic_rich_text_state_plan_test
  rich_text_state_plan_test.cpp)
target_link_libraries(canvas_semantic_rich_text_state_plan_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_rich_text_state_plan_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_rich_text_state_plan_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_rich_text_state_plan_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_rich_text_state_plan_test)

add_executable(canvas_semantic_apply_plan_test apply_plan_test.cpp)
target_link_libraries(canvas_semantic_apply_plan_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_apply_plan_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_apply_plan_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_apply_plan_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_apply_plan_test)

add_executable(canvas_semantic_atomic_apply_test atomic_apply_test.cpp)
target_link_libraries(canvas_semantic_atomic_apply_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_atomic_apply_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_atomic_apply_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_atomic_apply_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_atomic_apply_test)

add_executable(canvas_semantic_atomic_apply_differential_test atomic_apply_differential_test.cpp)
target_link_libraries(canvas_semantic_atomic_apply_differential_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_atomic_apply_differential_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(canvas_semantic_atomic_apply_differential_test
  PRIVATE ATOMIC_APPLY_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/atomic_apply.cpp")
if(MSVC)
  target_compile_options(canvas_semantic_atomic_apply_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_atomic_apply_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_atomic_apply_differential_test)

add_executable(canvas_semantic_operation_engine_boundary_test operation_engine_boundary_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_boundary_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_boundary_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_boundary_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_boundary_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_boundary_test)

add_executable(canvas_semantic_operation_engine_apply_test operation_engine_apply_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_apply_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_apply_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_apply_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_apply_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_apply_test)

add_executable(
  canvas_semantic_operation_engine_apply_differential_test
  operation_engine_apply_differential_test.cpp)
target_link_libraries(
  canvas_semantic_operation_engine_apply_differential_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(
  canvas_semantic_operation_engine_apply_differential_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(
  canvas_semantic_operation_engine_apply_differential_test
  PRIVATE OPERATION_ENGINE_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/operation_engine.cpp")
if(MSVC)
  target_compile_options(
    canvas_semantic_operation_engine_apply_differential_test
    PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(
    canvas_semantic_operation_engine_apply_differential_test
    PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_apply_differential_test)

add_executable(canvas_semantic_operation_engine_generation_change_set_test
  operation_engine_generation_change_set_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_generation_change_set_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_generation_change_set_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_generation_change_set_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_generation_change_set_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_generation_change_set_test)

add_executable(canvas_semantic_operation_engine_generation_change_set_differential_test
  operation_engine_generation_change_set_differential_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_generation_change_set_differential_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_generation_change_set_differential_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(canvas_semantic_operation_engine_generation_change_set_differential_test
  PRIVATE OPERATION_ENGINE_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/operation_engine.cpp"
          CHANGE_SET_BUILDER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/change_set_builder.cpp")
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_generation_change_set_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_generation_change_set_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_generation_change_set_differential_test)

add_executable(canvas_semantic_canonical_commit_clock_test canonical_commit_clock_test.cpp)
target_link_libraries(canvas_semantic_canonical_commit_clock_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_canonical_commit_clock_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_canonical_commit_clock_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_canonical_commit_clock_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_canonical_commit_clock_test)

add_executable(canvas_semantic_operation_engine_canonical_commit_test
  operation_engine_canonical_commit_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_canonical_commit_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_canonical_commit_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_canonical_commit_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_canonical_commit_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_canonical_commit_test)

add_executable(canvas_semantic_operation_engine_canonical_commit_differential_test
  operation_engine_canonical_commit_differential_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_canonical_commit_differential_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_canonical_commit_differential_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(canvas_semantic_operation_engine_canonical_commit_differential_test
  PRIVATE OPERATION_ENGINE_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/operation_engine.cpp")
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_canonical_commit_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_canonical_commit_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_canonical_commit_differential_test)

add_executable(canvas_semantic_operation_engine_publication_projection_test
  operation_engine_publication_projection_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_publication_projection_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_publication_projection_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_publication_projection_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_publication_projection_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_publication_projection_test)

add_executable(canvas_semantic_stateful_validation_differential_test stateful_validation_differential_test.cpp)
target_link_libraries(canvas_semantic_stateful_validation_differential_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_stateful_validation_differential_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_stateful_validation_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_stateful_validation_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_stateful_validation_differential_test)

add_executable(canvas_semantic_g1_04_b_operation_matrix_test g1_04_b_operation_matrix_test.cpp)
target_link_libraries(canvas_semantic_g1_04_b_operation_matrix_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_04_b_operation_matrix_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_g1_04_b_operation_matrix_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_04_b_operation_matrix_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_04_b_operation_matrix_test)

add_executable(canvas_semantic_g1_04_b_no_mutation_test g1_04_b_no_mutation_test.cpp)
target_link_libraries(canvas_semantic_g1_04_b_no_mutation_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_04_b_no_mutation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_g1_04_b_no_mutation_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_04_b_no_mutation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_04_b_no_mutation_test)

foreach(header IN ITEMS
        canvas/semantic/applied_operation_ledger.hpp
        canvas/semantic/apply_source.hpp
        canvas/semantic/canonical_numeric.hpp
        canvas/semantic/canonical_commit_stamp.hpp
        canvas/semantic/canonical_commit_clock.hpp
        canvas/semantic/canonical_commit_record.hpp
        canvas/semantic/change_set.hpp
        canvas/semantic/erase_mask.hpp
        canvas/semantic/object_content.hpp
        canvas/semantic/object_store.hpp
        canvas/semantic/object_record.hpp
        canvas/semantic/indexed_object_store.hpp
        canvas/semantic/property_value.hpp
        canvas/semantic/reference_object_store.hpp
        canvas/semantic/operation_id.hpp
        canvas/semantic/operation.hpp
        canvas/semantic/operation_fingerprint.hpp
        canvas/semantic/idempotency.hpp
        canvas/semantic/order_key.hpp
        canvas/semantic/semantic_generation.hpp
        canvas/semantic/semantic_geometry.hpp
        canvas/semantic/semantic_id.hpp
        canvas/semantic/semantic_error.hpp
        canvas/semantic/snapshot.hpp
        canvas/semantic/staged_object_view.hpp
        canvas/semantic/restore_planner.hpp
        canvas/semantic/operation_specific_validation.hpp
        canvas/semantic/apply_plan.hpp
        canvas/semantic/operation_engine.hpp
        canvas/semantic/replay.hpp
        canvas/semantic/stateful_validation.hpp
        canvas/semantic/hierarchy_validation.hpp
        canvas/semantic/operation_state_validator.hpp
        canvas/semantic/geometry_accounting_v1.hpp
        canvas/semantic/codec.hpp)
  string(REPLACE "/" "_" target_suffix ${header})
  string(REPLACE "." "_" target_suffix ${target_suffix})
  set(source ${CMAKE_CURRENT_BINARY_DIR}/self_contained_${target_suffix}.cpp)
  file(WRITE ${source} "#include \"${header}\"\n")
  add_library(canvas_semantic_${target_suffix} OBJECT ${source})
  target_link_libraries(canvas_semantic_${target_suffix} PRIVATE canvas_runtime_semantic)
  if(MSVC)
    target_compile_options(canvas_semantic_${target_suffix} PRIVATE /W4 /permissive- /WX /EHsc)
  else()
    target_compile_options(canvas_semantic_${target_suffix} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(canvas_semantic_g1_04_c_fixture_decoder_test
  g1_04_c_fixture_decoder_test.cpp
  ../tools/g1_04_c_fixture_decoder.cpp)
target_include_directories(canvas_semantic_g1_04_c_fixture_decoder_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_04_c_fixture_decoder_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_04_c_fixture_decoder_test PRIVATE G1_C_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(canvas_semantic_g1_04_c_fixture_decoder_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
gtest_discover_tests(canvas_semantic_g1_04_c_fixture_decoder_test)

add_executable(canvas_semantic_g1_04_c_projection_test
  g1_04_c_projection_test.cpp
  ../tools/g1_04_c_fixture_decoder.cpp
  ../tools/g1_04_c_fixture_runtime.cpp
  ../tools/g1_04_c_projection.cpp
  ../tools/g1_04_c_observer.cpp)
target_include_directories(canvas_semantic_g1_04_c_projection_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_04_c_projection_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_04_c_projection_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_04_c_projection_test PRIVATE G1_C_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(canvas_semantic_g1_04_c_projection_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
gtest_discover_tests(canvas_semantic_g1_04_c_projection_test)

add_executable(canvas_semantic_g1_06_projection_test
  g1_06_projection_test.cpp
  ../tools/g1_06_projection.cpp)
target_include_directories(canvas_semantic_g1_06_projection_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_06_projection_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_06_projection_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_06_projection_test PRIVATE
  G1_06_PROJECTION_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../tools/g1_06_projection.cpp"
  G1_06_PROJECTION_HEADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../tools/g1_06_projection.hpp")
target_link_libraries(canvas_semantic_g1_06_projection_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_projection_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_projection_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_projection_test TEST_PREFIX "g1_06_projection.")

add_executable(canvas_semantic_g1_06_projection_differential_test
  g1_06_projection_differential_test.cpp
  ../tools/g1_06_projection.cpp)
target_include_directories(canvas_semantic_g1_06_projection_differential_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_06_projection_differential_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_06_projection_differential_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_link_libraries(canvas_semantic_g1_06_projection_differential_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_projection_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_projection_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_projection_differential_test TEST_PREFIX "g1_06_projection.")

add_executable(canvas_semantic_g1_06_replay_test
  g1_06_replay_test.cpp)
target_link_libraries(canvas_semantic_g1_06_replay_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_06_replay_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_replay_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_replay_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_replay_test TEST_PREFIX "g1_06_replay.")

add_executable(canvas_semantic_g1_06_replay_differential_test
  g1_06_replay_differential_test.cpp
  ../tools/g1_06_projection.cpp)
target_link_libraries(canvas_semantic_g1_06_replay_differential_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_g1_06_replay_differential_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
          ${CMAKE_CURRENT_SOURCE_DIR}/../tools
          ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_06_replay_differential_test
  PRIVATE G1_06_REPLAY_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/replay.cpp")
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_replay_differential_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_replay_differential_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_replay_differential_test TEST_PREFIX "g1_06_replay.")

add_executable(canvas_semantic_g1_06_digest_test
  g1_06_digest_test.cpp
  ../tools/g1_06_projection.cpp
  ../tools/g1_06_digest.cpp)
target_include_directories(canvas_semantic_g1_06_digest_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_06_digest_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_06_digest_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_06_digest_test PRIVATE
  G1_06_DIGEST_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../tools/g1_06_digest.cpp"
  G1_06_DIGEST_HEADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../tools/g1_06_digest.hpp")
target_link_libraries(canvas_semantic_g1_06_digest_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
if(MSVC)
  target_compile_options(canvas_semantic_g1_06_digest_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_06_digest_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_g1_06_digest_test TEST_PREFIX "g1_06_digest.")

add_executable(canvas_semantic_g1_04_c_observer_contract_test
  g1_04_c_observer_contract_test.cpp
  ../tools/g1_04_c_fixture_decoder.cpp
  ../tools/g1_04_c_fixture_runtime.cpp
  ../tools/g1_04_c_projection.cpp
  ../tools/g1_04_c_observer.cpp)
target_include_directories(canvas_semantic_g1_04_c_observer_contract_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_04_c_observer_contract_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_04_c_observer_contract_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_04_c_observer_contract_test PRIVATE G1_C_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(canvas_semantic_g1_04_c_observer_contract_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
gtest_discover_tests(canvas_semantic_g1_04_c_observer_contract_test)

add_executable(canvas_semantic_g1_04_c_observer_coverage_test
  g1_04_c_observer_coverage_test.cpp
  ../tools/g1_04_c_fixture_decoder.cpp
  ../tools/g1_04_c_fixture_runtime.cpp
  ../tools/g1_04_c_projection.cpp
  ../tools/g1_04_c_observer.cpp)
target_include_directories(canvas_semantic_g1_04_c_observer_coverage_test PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_04_c_observer_coverage_test PRIVATE ../src)
target_include_directories(canvas_semantic_g1_04_c_observer_coverage_test PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_compile_definitions(canvas_semantic_g1_04_c_observer_coverage_test PRIVATE G1_C_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(canvas_semantic_g1_04_c_observer_coverage_test PRIVATE canvas_runtime_semantic GTest::gtest_main)
gtest_discover_tests(canvas_semantic_g1_04_c_observer_coverage_test)

add_executable(canvas_semantic_g1_04_c_observation_export
  ../tools/g1_04_c_observation_export.cpp
  ../tools/g1_04_c_fixture_decoder.cpp
  ../tools/g1_04_c_fixture_runtime.cpp
  ../tools/g1_04_c_projection.cpp
  ../tools/g1_04_c_observer.cpp)
target_include_directories(canvas_semantic_g1_04_c_observation_export PRIVATE ../tools)
target_include_directories(canvas_semantic_g1_04_c_observation_export PRIVATE ../src)
target_include_directories(canvas_semantic_g1_04_c_observation_export PRIVATE ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
target_link_libraries(canvas_semantic_g1_04_c_observation_export PRIVATE canvas_runtime_semantic)

add_test(
    NAME canvas_semantic_runtime_boundaries
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_runtime_boundaries.py
            --root ${PROJECT_SOURCE_DIR})

add_executable(canvas_semantic_operation_engine_15_operation_conformance_test
  operation_engine_15_operation_conformance_test.cpp)
target_link_libraries(canvas_semantic_operation_engine_15_operation_conformance_test
  PRIVATE canvas_runtime_semantic GTest::gtest_main)
target_include_directories(canvas_semantic_operation_engine_15_operation_conformance_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_operation_engine_15_operation_conformance_test PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_operation_engine_15_operation_conformance_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
gtest_discover_tests(canvas_semantic_operation_engine_15_operation_conformance_test)
