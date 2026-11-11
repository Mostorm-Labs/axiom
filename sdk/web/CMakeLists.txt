include(${PROJECT_SOURCE_DIR}/cmake/AxiomHostTextFont.cmake)
axiom_resolve_host_text_font()

add_library(axiom_sdk_web_runtime_adapter STATIC web_runtime_adapter.cpp)
target_link_libraries(axiom_sdk_web_runtime_adapter PUBLIC canvas_runtime_composition)
target_include_directories(axiom_sdk_web_runtime_adapter PRIVATE
  "${PROJECT_SOURCE_DIR}/apps/minimal_runtime_host" "${AXIOM_HOST_FONT_INCLUDE}")
target_compile_features(axiom_sdk_web_runtime_adapter PUBLIC cxx_std_20)
target_compile_options(axiom_sdk_web_runtime_adapter PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(axiom_sdk_web_runtime_adapter PROPERTIES OUTPUT_NAME axiom_web_runtime_adapter)

add_executable(axiom_js_runtime bridge.cpp)
target_link_libraries(axiom_js_runtime PRIVATE axiom_sdk_web_runtime_adapter axiom_sdk_session)
target_compile_features(axiom_js_runtime PRIVATE cxx_std_20)
target_compile_options(axiom_js_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror -fexceptions)
target_include_directories(axiom_js_runtime PRIVATE
  "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include"
  "${PROJECT_SOURCE_DIR}/sdk/common")
set(_axiom_web_exports "'_axiom_js_call','_axiom_js_bytes','_axiom_js_size','_malloc','_free'")
if(AXIOM_G5_UR0_DIAGNOSTICS)
  string(APPEND _axiom_web_exports ",'_axiom_g5_trace_control','_axiom_g5_trace_bytes','_axiom_g5_trace_size'")
endif()
target_link_options(axiom_js_runtime PRIVATE
  -fexceptions
  --no-entry -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
  "-sEXPORTED_FUNCTIONS=[${_axiom_web_exports}]"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToNewUTF8','stringToUTF16','HEAPU8']"
  "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf@/axiom-font.otf\""
  "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts/imported/catalog.json@/fonts/catalog.json\""
  "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts/imported/Excalifont@/fonts/Excalifont\"")
set_target_properties(axiom_js_runtime PROPERTIES OUTPUT_NAME axiom SUFFIX .mjs
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/internal")
add_custom_command(TARGET axiom_js_runtime POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/internal/axiom.wasm" "${CMAKE_CURRENT_BINARY_DIR}/axiom.wasm"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/internal/axiom.data" "${CMAKE_CURRENT_BINARY_DIR}/axiom.data")
file(COPY "${PROJECT_SOURCE_DIR}/sdk/api/axiom.mjs" "${PROJECT_SOURCE_DIR}/sdk/api/axiom.d.mts"
  "${PROJECT_SOURCE_DIR}/sdk/api/demo-controls.mjs" "${PROJECT_SOURCE_DIR}/sdk/api/demo-controls.d.mts"
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/api")
file(COPY "${PROJECT_SOURCE_DIR}/sdk/api/internal" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/api")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/api" DESTINATION js)
foreach(file axiom.mjs axiom.d.mts package.json README.md example.html)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${file}" _web_source)
  string(REPLACE "'../api/" "'./api/" _web_source "${_web_source}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${file}" "${_web_source}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${file}" DESTINATION js)
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/release/version_identity.py"
  --repo "${PROJECT_SOURCE_DIR}" --output "${CMAKE_CURRENT_BINARY_DIR}" --build-number "${AXIOM_BUILD_NUMBER}"
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_QUIET)
configure_file("${PROJECT_SOURCE_DIR}/verification/sdk_web_external/consumer/pointer_input.mjs" "${CMAKE_CURRENT_BINARY_DIR}/pointer_input.mjs" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/text_input.mjs" "${CMAKE_CURRENT_BINARY_DIR}/text_input.mjs" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/verification/sdk_web_external/consumer/serve.py" "${CMAKE_CURRENT_BINARY_DIR}/serve.py" COPYONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/serve.py" DESTINATION js)
get_filename_component(_js_semantic_root "${Protobuf_DIR}/../../.." ABSOLUTE)
file(COPY "${_js_semantic_root}/licenses/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/licenses/semantic")
file(COPY "${CANVAS_SKIA_SDK_ROOT}/licenses/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/licenses/skia")
configure_file("${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/LICENSE.MIT" "${CMAKE_CURRENT_BINARY_DIR}/licenses/nlohmann-json-LICENSE.MIT" COPYONLY)
foreach(name LICENSE.txt NOTICE.txt)
  configure_file("${PROJECT_SOURCE_DIR}/assets/fonts/noto-sans-cjk-sc/${name}" "${CMAKE_CURRENT_BINARY_DIR}/licenses/noto-cjk-${name}" COPYONLY)
endforeach()
file(GLOB_RECURSE _js_font_files "${PROJECT_SOURCE_DIR}/assets/fonts/imported/Excalifont/*")
foreach(file IN LISTS _js_font_files)
  get_filename_component(name "${file}" NAME)
  string(TOLOWER "${name}" lower_name)
  if(lower_name MATCHES "license|ofl|notice")
    file(RELATIVE_PATH relative "${PROJECT_SOURCE_DIR}/assets/fonts/imported" "${file}")
    configure_file("${file}" "${CMAKE_CURRENT_BINARY_DIR}/licenses/fonts/${relative}" COPYONLY)
  endif()
endforeach()
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/licenses" DESTINATION js)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pointer_input.mjs" "${CMAKE_CURRENT_BINARY_DIR}/text_input.mjs" DESTINATION js)
install(FILES "$<TARGET_FILE:axiom_js_runtime>" DESTINATION js/internal)
if(AXIOM_INSTALL_DEVELOPER_COMPONENTS AND CANVAS_SEMANTIC_ENABLE_PROTOBUF AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS
   AND CMAKE_BUILD_TYPE STREQUAL "Release")
  install(TARGETS axiom_sdk_web_runtime_adapter ARCHIVE DESTINATION lib)
endif()
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/axiom.wasm"
  "${CMAKE_CURRENT_BINARY_DIR}/axiom.data" DESTINATION js)

# Explicit verification probe; excluded from ordinary SDK builds/install.
option(AXIOM_BUILD_G5_COVERAGE_PROBE "Build private exact coverage browser probe" OFF)
if(AXIOM_BUILD_G5_COVERAGE_PROBE)
  add_executable(g5_coverage_probe "${PROJECT_SOURCE_DIR}/runtime/render/tests/render_cache_test.cpp")
  target_link_libraries(g5_coverage_probe PRIVATE canvas_runtime_render)
  target_include_directories(g5_coverage_probe PRIVATE "${CANVAS_SKIA_SDK_ROOT}")
  target_compile_definitions(g5_coverage_probe PRIVATE G5_TEST_WEBGL=1)
  # The accepted Skia SDK omits RTTI; the private SkNoDrawCanvas oracle
  # must use the same policy. This target is never a consumer artifact.
  target_compile_options(g5_coverage_probe PRIVATE -fexceptions -fno-rtti)
  target_compile_features(g5_coverage_probe PRIVATE cxx_std_20)
  target_link_options(g5_coverage_probe PRIVATE -fexceptions -sASYNCIFY=1 -sMODULARIZE=1 -sEXPORT_NAME=createCacheProbe -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sEXIT_RUNTIME=1)
endif()
