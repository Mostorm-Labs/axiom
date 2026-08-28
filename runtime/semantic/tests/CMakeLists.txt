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

foreach(header IN ITEMS
        canvas/semantic/canonical_numeric.hpp
        canvas/semantic/canonical_commit_stamp.hpp
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
        canvas/semantic/order_key.hpp
        canvas/semantic/semantic_generation.hpp
        canvas/semantic/semantic_geometry.hpp
        canvas/semantic/semantic_id.hpp
        canvas/semantic/semantic_error.hpp
        canvas/semantic/staged_object_view.hpp
        canvas/semantic/stateful_validation.hpp
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
add_test(
    NAME canvas_semantic_runtime_boundaries
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_runtime_boundaries.py
            --root ${PROJECT_SOURCE_DIR})
