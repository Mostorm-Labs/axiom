add_library(canvas_runtime_composition STATIC
    src/resource_binding.cpp
    src/scene_adapter.cpp
    src/runtime_composition.cpp)
target_include_directories(canvas_runtime_composition PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/runtime/semantic/src)
target_link_libraries(canvas_runtime_composition PUBLIC
    canvas_runtime_editor canvas_runtime_scene canvas_runtime_render
    canvas_runtime_text canvas_runtime_foundation)
target_compile_features(canvas_runtime_composition PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(canvas_runtime_composition PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_runtime_composition PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
  add_executable(axiom_fc00x_runtime_composition_tests
      tests/runtime_composition_test.cpp)
  target_link_libraries(axiom_fc00x_runtime_composition_tests PRIVATE canvas_runtime_composition)
  target_compile_features(axiom_fc00x_runtime_composition_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_fc00x_runtime_composition_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_fc00x_runtime_composition_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
  endif()
  add_test(NAME FC00XRuntimeComposition COMMAND axiom_fc00x_runtime_composition_tests)
endif()
