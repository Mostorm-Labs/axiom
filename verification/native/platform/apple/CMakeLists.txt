add_library(axiom_verification_platform_apple STATIC
  src/apple_harness_adapter.cpp
  src/apple_input_bridge.cpp
  src/apple_profile.cpp
)
if(APPLE)
  find_library(APPLE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(APPLE_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
  target_sources(axiom_verification_platform_apple PRIVATE src/apple_surface_host.mm)
  target_link_libraries(axiom_verification_platform_apple PUBLIC
    ${APPLE_FOUNDATION_FRAMEWORK} ${APPLE_QUARTZCORE_FRAMEWORK})
else()
  target_sources(axiom_verification_platform_apple PRIVATE src/apple_surface_host_stub.cpp)
endif()
target_include_directories(axiom_verification_platform_apple PUBLIC include)
target_link_libraries(axiom_verification_platform_apple PUBLIC
  axiom_verification_platform_hooks
  axiom_verification_platform_host_common
)
target_compile_features(axiom_verification_platform_apple PUBLIC cxx_std_20)

add_executable(axiom_verification_platform_apple_test
  tests/apple_harness_adapter_test.cpp
)
target_link_libraries(axiom_verification_platform_apple_test PRIVATE axiom_verification_platform_apple)
add_test(NAME axiom_verification_platform_apple_test COMMAND axiom_verification_platform_apple_test)
