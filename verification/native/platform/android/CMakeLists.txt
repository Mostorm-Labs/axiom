add_library(axiom_verification_platform_android STATIC
  src/android_harness_adapter.cpp
  src/android_input_bridge.cpp
  src/android_profile.cpp
)
target_include_directories(axiom_verification_platform_android PUBLIC include)
target_link_libraries(axiom_verification_platform_android PUBLIC
  axiom_verification_platform_hooks
  axiom_verification_platform_host_common
)
target_compile_features(axiom_verification_platform_android PUBLIC cxx_std_20)

add_executable(axiom_verification_platform_android_test
  tests/android_harness_adapter_test.cpp
)
target_link_libraries(axiom_verification_platform_android_test PRIVATE
  axiom_verification_platform_android
)
add_test(NAME axiom_verification_platform_android_test
  COMMAND axiom_verification_platform_android_test)

if(ANDROID)
  add_library(axiom_verification_android_jni SHARED
    instrumentation/jni/android_harness_jni.cpp
  )
  # Android 15+ 16 KB page-size devices require ELF LOAD segments to be
  # linked on 16 KB boundaries. Keep this explicit for all arm64/x86_64
  # verification APKs; APK zip alignment alone is not sufficient.
  target_link_options(axiom_verification_android_jni PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
  )
  target_link_libraries(axiom_verification_android_jni PRIVATE
    axiom_verification_platform_android
    android
    log
  )
  target_compile_features(axiom_verification_android_jni PRIVATE cxx_std_20)
endif()
