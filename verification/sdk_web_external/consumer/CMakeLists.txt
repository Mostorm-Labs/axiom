cmake_minimum_required(VERSION 3.24)
project(AxiomExternalWeb LANGUAGES CXX)
find_package(AxiomRuntime CONFIG REQUIRED)
add_executable(external_web main.cpp)
target_link_libraries(external_web PRIVATE Axiom::PlatformWeb)
target_compile_features(external_web PRIVATE cxx_std_20)
target_link_options(external_web PRIVATE "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
  "-sALLOW_MEMORY_GROWTH=1" "-sEXPORTED_FUNCTIONS=['_main','_sdk_input']"
  "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/axiom-font.otf@/axiom-font.otf\""
  "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/fonts@/fonts\"")
set_target_properties(external_web PROPERTIES SUFFIX ".js")
