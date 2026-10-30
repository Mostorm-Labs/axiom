add_executable(axiom_js_runtime bridge.cpp)
target_link_libraries(axiom_js_runtime PRIVATE axiom_web_runtime_adapter canvas_runtime_composition)
target_compile_features(axiom_js_runtime PRIVATE cxx_std_20)
target_compile_options(axiom_js_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror -fexceptions)
target_include_directories(axiom_js_runtime PRIVATE
  "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include"
  "${PROJECT_SOURCE_DIR}/apps/axiom_canvas_demo/platform/web")
target_link_options(axiom_js_runtime PRIVATE
  -fexceptions
  --no-entry -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
  "-sEXPORTED_FUNCTIONS=['_axiom_js_call','_axiom_js_bytes','_axiom_js_size','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToNewUTF8','stringToUTF16','HEAPU8']"
  "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf@/axiom-font.otf\""
  "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts/imported@/fonts\"")
set_target_properties(axiom_js_runtime PROPERTIES OUTPUT_NAME axiom-runtime SUFFIX .mjs)
foreach(file axiom.mjs axiom.d.mts package.json README.md example.html)
  configure_file("${file}" "${CMAKE_CURRENT_BINARY_DIR}/${file}" COPYONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${file}" DESTINATION js)
endforeach()
configure_file("${PROJECT_SOURCE_DIR}/verification/sdk_web_external/consumer/pointer_input.mjs" "${CMAKE_CURRENT_BINARY_DIR}/pointer_input.mjs" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/apps/axiom_canvas_demo/platform/web/text_input.js" "${CMAKE_CURRENT_BINARY_DIR}/text_input.mjs" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/verification/sdk_web_external/consumer/serve.py" "${CMAKE_CURRENT_BINARY_DIR}/serve.py" COPYONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/serve.py" DESTINATION js)
get_filename_component(_js_semantic_root "${Protobuf_DIR}/../../.." ABSOLUTE)
file(COPY "${_js_semantic_root}/licenses/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/licenses/semantic")
file(COPY "${CANVAS_SKIA_SDK_ROOT}/licenses/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/licenses/skia")
configure_file("${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/LICENSE.MIT" "${CMAKE_CURRENT_BINARY_DIR}/licenses/nlohmann-json-LICENSE.MIT" COPYONLY)
foreach(name LICENSE.txt NOTICE.txt)
  configure_file("${PROJECT_SOURCE_DIR}/assets/fonts/noto-sans-cjk-sc/${name}" "${CMAKE_CURRENT_BINARY_DIR}/licenses/noto-cjk-${name}" COPYONLY)
endforeach()
file(GLOB_RECURSE _js_font_files "${PROJECT_SOURCE_DIR}/assets/fonts/imported/*")
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
install(FILES "$<TARGET_FILE:axiom_js_runtime>" "${CMAKE_CURRENT_BINARY_DIR}/axiom-runtime.wasm"
  "${CMAKE_CURRENT_BINARY_DIR}/axiom-runtime.data" DESTINATION js)
