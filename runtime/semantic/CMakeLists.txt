add_library(canvas_runtime_semantic STATIC
    src/semantic_types.cpp
    src/codec.cpp)
target_include_directories(canvas_runtime_semantic PUBLIC include)
target_link_libraries(canvas_runtime_semantic PUBLIC canvas_runtime_foundation)
target_compile_features(canvas_runtime_semantic PUBLIC cxx_std_20)

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
