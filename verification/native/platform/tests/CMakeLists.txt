find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(axiom_verification_platform_hooks_test
  platform_hooks_test.cpp
)
target_link_libraries(axiom_verification_platform_hooks_test PRIVATE
  axiom_verification_platform_hooks
)
target_compile_features(axiom_verification_platform_hooks_test PRIVATE cxx_std_20)
add_test(NAME axiom_verification_platform_hooks_test
  COMMAND axiom_verification_platform_hooks_test)

add_executable(axiom_verification_platform_host_common_test
  platform_host_common_test.cpp
)
target_link_libraries(axiom_verification_platform_host_common_test PRIVATE
  axiom_verification_platform_host_common
)
target_compile_features(axiom_verification_platform_host_common_test PRIVATE cxx_std_20)
add_test(NAME axiom_verification_platform_host_common_test
  COMMAND axiom_verification_platform_host_common_test)

add_test(NAME axiom_verification_build_boundary
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check_build_boundary.py
    --root ${CMAKE_SOURCE_DIR})

add_test(NAME axiom_verification_native_trace
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/validate_native_trace.py
    --hooks $<TARGET_FILE:axiom_verification_platform_hooks_test>
    --host $<TARGET_FILE:axiom_verification_platform_host_common_test>
    --output ${CMAKE_CURRENT_BINARY_DIR}/trace)
