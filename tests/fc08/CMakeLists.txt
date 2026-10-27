if(NOT TARGET canvas_runtime_semantic OR NOT TARGET canvas_runtime_composition)
  message(FATAL_ERROR "FC08 verification requires the existing Semantic and Composition consumers")
endif()
add_executable(axiom_fc08_headless_probe native/headless_probe.cpp)
target_link_libraries(axiom_fc08_headless_probe PRIVATE canvas_runtime_semantic)
target_compile_features(axiom_fc08_headless_probe PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(axiom_fc08_headless_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_fc08_headless_probe PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
if(EMSCRIPTEN)
  set_target_properties(axiom_fc08_headless_probe PROPERTIES SUFFIX ".js")
  target_link_options(axiom_fc08_headless_probe PRIVATE
    -sMODULARIZE=1 -sEXPORT_NAME=FC08Headless -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=['FS','callMain'] -sINVOKE_RUN=0)
endif()
foreach(regression IN ITEMS selection connector control)
  if(regression STREQUAL "selection")
    set(regression_source "${PROJECT_SOURCE_DIR}/runtime/interaction/tests/selection_session_test.cpp")
    set(regression_export FC08Selection)
  elseif(regression STREQUAL "connector")
    set(regression_source "${PROJECT_SOURCE_DIR}/runtime/composition/tests/fc05_connector_facade_test.cpp")
    # The retained source declares size_t and uint64_t in one `auto` declaration;
    # these differ on wasm32. Split only that declaration in a build-local copy.
    # No original test or production source is modified and assertions stay on.
    file(READ "${regression_source}" regression_text)
    string(REPLACE
      "const auto count=c.semanticObjectCount(),before=c.readback().semanticGeneration;"
      "const auto count=c.semanticObjectCount(); const auto before=c.readback().semanticGeneration;"
      regression_text "${regression_text}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/connector_regression_portable.cpp" "${regression_text}")
    set(regression_source "${CMAKE_CURRENT_BINARY_DIR}/connector_regression_portable.cpp")
    set(regression_export FC08Connector)
  else()
    set(regression_source "${PROJECT_SOURCE_DIR}/runtime/composition/tests/canvas_runtime_facade_test.cpp")
    set(regression_export FC08Control)
  endif()
  add_executable(axiom_fc08_${regression}_regression "${regression_source}")
  target_link_libraries(axiom_fc08_${regression}_regression PRIVATE canvas_runtime_composition)
  target_compile_features(axiom_fc08_${regression}_regression PRIVATE cxx_std_20)
  if(regression STREQUAL "control")
    # Match this retained test's original CMake qualification definition. This
    # does not expose private APIs to the ordinary public-consumer probe.
    target_compile_definitions(axiom_fc08_${regression}_regression PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
  endif()
  if(MSVC)
    target_compile_options(axiom_fc08_${regression}_regression PRIVATE /UNDEBUG /W4 /permissive- /WX /EHsc)
  else()
    target_compile_options(axiom_fc08_${regression}_regression PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(EMSCRIPTEN)
    set_target_properties(axiom_fc08_${regression}_regression PROPERTIES SUFFIX ".js")
    target_link_options(axiom_fc08_${regression}_regression PRIVATE
      -sMODULARIZE=1 -sEXPORT_NAME=${regression_export} -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
      -sEXPORTED_RUNTIME_METHODS=['callMain'] -sINVOKE_RUN=0)
  else()
    add_test(NAME FC08Inherited_${regression} COMMAND axiom_fc08_${regression}_regression)
  endif()
endforeach()
add_executable(axiom_fc08_hook_probe native/hook_probe.cpp)
target_link_libraries(axiom_fc08_hook_probe PRIVATE canvas_runtime_composition canvas_runtime_semantic)
target_compile_features(axiom_fc08_hook_probe PRIVATE cxx_std_20)
target_include_directories(axiom_fc08_hook_probe PRIVATE
  "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include"
  "${PROJECT_SOURCE_DIR}/runtime/semantic/src"
  "${PROJECT_SOURCE_DIR}/runtime/interaction/src")
target_compile_definitions(axiom_fc08_hook_probe PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
if(MSVC)
  target_compile_options(axiom_fc08_hook_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_fc08_hook_probe PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
if(EMSCRIPTEN)
  set_target_properties(axiom_fc08_hook_probe PROPERTIES SUFFIX ".js")
  target_link_options(axiom_fc08_hook_probe PRIVATE
    -sMODULARIZE=1 -sEXPORT_NAME=FC08Hooks -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=['FS','callMain'] -sINVOKE_RUN=0)
endif()
add_executable(axiom_fc08_baseline_probe native/baseline_probe.cpp native/public_consumer.cpp)
target_link_libraries(axiom_fc08_baseline_probe PRIVATE canvas_runtime_semantic canvas_runtime_composition)
target_compile_features(axiom_fc08_baseline_probe PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(axiom_fc08_baseline_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_fc08_baseline_probe PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
if(EMSCRIPTEN)
  set_target_properties(axiom_fc08_baseline_probe PROPERTIES SUFFIX ".js")
  target_link_options(axiom_fc08_baseline_probe PRIVATE
    -sMODULARIZE=1 -sEXPORT_NAME=FC08Probe -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=['FS','callMain'] -sINVOKE_RUN=0)
else()
  add_test(NAME FC08BaselineProbeDiscovery COMMAND axiom_fc08_baseline_probe --discover)
endif()
