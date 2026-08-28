add_library(canvas_runtime_semantic STATIC
    src/change_set.cpp
    src/indexed_object_store.cpp
    src/idempotency.cpp
    src/object_index.cpp
    src/reference_object_store.cpp
    src/semantic_types.cpp
    src/normalizer.cpp
    src/operation_fingerprint.cpp
    src/operation_state_validator.cpp
    src/staged_object_view.cpp
    src/hierarchy_validation.cpp
    src/stateful_validation.cpp
    src/validator.cpp
    src/codec.cpp)
target_include_directories(canvas_runtime_semantic PUBLIC include)
target_link_libraries(canvas_runtime_semantic PUBLIC canvas_runtime_foundation)
target_compile_features(canvas_runtime_semantic PUBLIC cxx_std_20)

if(CANVAS_SEMANTIC_ENABLE_PROTOBUF)
  find_package(Protobuf CONFIG REQUIRED)
  if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc)
    message(FATAL_ERROR "Protobuf CONFIG package must provide protobuf::libprotobuf and protobuf::protoc")
  endif()
  set(AXIOM_PROTO_ROOT ${PROJECT_SOURCE_DIR}/schema/axiom/v1/proto)
  set(AXIOM_PROTO_GENERATED_ROOT ${CMAKE_CURRENT_BINARY_DIR}/generated)
  file(GLOB_RECURSE AXIOM_PROTO_FILES CONFIGURE_DEPENDS
       "${AXIOM_PROTO_ROOT}/*.proto")
  set(AXIOM_PROTO_RELATIVE_FILES)
  set(AXIOM_PROTO_GENERATED_CC)
  set(AXIOM_PROTO_GENERATED_H)
  foreach(proto IN LISTS AXIOM_PROTO_FILES)
    file(RELATIVE_PATH relative_proto "${AXIOM_PROTO_ROOT}" "${proto}")
    list(APPEND AXIOM_PROTO_RELATIVE_FILES "${relative_proto}")
    string(REGEX REPLACE "\\.proto$" ".pb.cc" generated_cc "${relative_proto}")
    string(REGEX REPLACE "\\.proto$" ".pb.h" generated_h "${relative_proto}")
    list(APPEND AXIOM_PROTO_GENERATED_CC "${AXIOM_PROTO_GENERATED_ROOT}/${generated_cc}")
    list(APPEND AXIOM_PROTO_GENERATED_H "${AXIOM_PROTO_GENERATED_ROOT}/${generated_h}")
  endforeach()
  if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    set_source_files_properties(${AXIOM_PROTO_GENERATED_CC}
      PROPERTIES COMPILE_OPTIONS "-Wno-nullability-extension;-Wno-deprecated-declarations")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(${AXIOM_PROTO_GENERATED_CC}
      PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
  endif()
  add_custom_command(
    OUTPUT ${AXIOM_PROTO_GENERATED_CC} ${AXIOM_PROTO_GENERATED_H}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AXIOM_PROTO_GENERATED_ROOT}"
    COMMAND $<TARGET_FILE:protobuf::protoc>
            --proto_path=${AXIOM_PROTO_ROOT}
            --cpp_out=${AXIOM_PROTO_GENERATED_ROOT}
            ${AXIOM_PROTO_RELATIVE_FILES}
    DEPENDS protobuf::protoc ${AXIOM_PROTO_FILES}
    WORKING_DIRECTORY "${AXIOM_PROTO_ROOT}"
    VERBATIM)
  target_sources(canvas_runtime_semantic PRIVATE ${AXIOM_PROTO_GENERATED_CC})
  target_include_directories(canvas_runtime_semantic PRIVATE "${AXIOM_PROTO_GENERATED_ROOT}")
  target_link_libraries(canvas_runtime_semantic PRIVATE protobuf::libprotobuf)
  target_compile_definitions(canvas_runtime_semantic PUBLIC CANVAS_SEMANTIC_PROTOBUF=1)
  add_subdirectory(tools)
endif()

if(MSVC)
  target_compile_options(canvas_runtime_semantic PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_runtime_semantic PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CANVAS_BUILD_SEMANTIC AND BUILD_TESTING)
  if(NOT EXISTS ${PROJECT_SOURCE_DIR}/.deps/googletest/CMakeLists.txt)
    message(FATAL_ERROR "Missing GoogleTest. Run: python3 tools/bootstrap_deps.py --core")
  endif()
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
  add_subdirectory(${PROJECT_SOURCE_DIR}/.deps/googletest
                   ${CMAKE_BINARY_DIR}/_deps/googletest EXCLUDE_FROM_ALL)
  add_subdirectory(tests)
endif()
