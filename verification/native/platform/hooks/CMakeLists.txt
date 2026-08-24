add_library(axiom_verification_platform_hooks STATIC
  src/event_tap.cpp
  src/fault_hook_port.cpp
  src/source_attempt_probe.cpp
  src/source_lease_probe.cpp
)
target_include_directories(axiom_verification_platform_hooks PUBLIC include)
target_compile_features(axiom_verification_platform_hooks PUBLIC cxx_std_20)
set_target_properties(axiom_verification_platform_hooks PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(MSVC)
  target_compile_options(axiom_verification_platform_hooks PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_verification_platform_hooks PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
