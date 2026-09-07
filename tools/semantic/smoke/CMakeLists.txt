cmake_minimum_required(VERSION 3.30)
project(axiom_semantic_sdk_smoke LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Protobuf CONFIG REQUIRED)
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "The selected target runtime lacks protobuf::libprotobuf")
endif()
set(AXIOM_PROTOC "" CACHE FILEPATH "Host protoc executable")
if(NOT AXIOM_PROTOC OR NOT EXISTS "${AXIOM_PROTOC}")
  message(FATAL_ERROR "Source-free smoke requires a separate host protoc")
endif()
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/smoke.pb.cc" "${CMAKE_CURRENT_BINARY_DIR}/smoke.pb.h"
  COMMAND "${AXIOM_PROTOC}"
          "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
          "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/smoke.proto"
  DEPENDS "${AXIOM_PROTOC}" "${CMAKE_CURRENT_SOURCE_DIR}/smoke.proto"
  VERBATIM)
add_executable(semantic_sdk_smoke main.cpp "${CMAKE_CURRENT_BINARY_DIR}/smoke.pb.cc")
target_include_directories(semantic_sdk_smoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(semantic_sdk_smoke PRIVATE protobuf::libprotobuf)
