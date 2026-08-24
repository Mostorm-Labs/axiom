add_library(axiom_verification_platform_host_common STATIC
  src/build_manifest.cpp
  src/protocol_bridge.cpp
)
target_include_directories(axiom_verification_platform_host_common PUBLIC include)
target_compile_features(axiom_verification_platform_host_common PUBLIC cxx_std_20)
set_target_properties(axiom_verification_platform_host_common PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(MSVC)
  target_compile_options(axiom_verification_platform_host_common PRIVATE
    /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_verification_platform_host_common PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
