add_library(axiom_verification_platform_windows STATIC
  src/windows_harness_adapter.cpp
  src/windows_input_bridge.cpp
  src/windows_profile.cpp
  src/windows_surface_host.cpp
)
target_include_directories(axiom_verification_platform_windows PUBLIC include)
target_link_libraries(axiom_verification_platform_windows PUBLIC
  axiom_verification_platform_hooks
  axiom_verification_platform_host_common
)
if(WIN32)
  target_link_libraries(axiom_verification_platform_windows PRIVATE d3d12 dxgi user32)
endif()
target_compile_features(axiom_verification_platform_windows PUBLIC cxx_std_20)

add_executable(axiom_verification_platform_windows_test
  tests/windows_harness_adapter_test.cpp
)
target_link_libraries(axiom_verification_platform_windows_test PRIVATE
  axiom_verification_platform_windows
)
add_test(NAME axiom_verification_platform_windows_test
  COMMAND axiom_verification_platform_windows_test)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AXIOM_WINDOWS_TRACE_FLAGS)
if(WIN32)
  list(APPEND AXIOM_WINDOWS_TRACE_FLAGS --require-native)
endif()
add_test(NAME axiom_verification_platform_windows_trace
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_windows_trace.py
    --probe $<TARGET_FILE:axiom_verification_platform_windows_test>
    --output ${CMAKE_CURRENT_BINARY_DIR}/trace/windows-adapter-trace.json
    ${AXIOM_WINDOWS_TRACE_FLAGS})
