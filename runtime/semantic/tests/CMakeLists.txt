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

foreach(header IN ITEMS
        canvas/semantic/canonical_numeric.hpp
        canvas/semantic/canonical_commit_stamp.hpp
        canvas/semantic/change_set.hpp
        canvas/semantic/object_record.hpp
        canvas/semantic/operation_id.hpp
        canvas/semantic/operation.hpp
        canvas/semantic/order_key.hpp
        canvas/semantic/semantic_generation.hpp
        canvas/semantic/semantic_error.hpp
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
