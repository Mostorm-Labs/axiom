cmake_minimum_required(VERSION 3.24)
project(AxiomExternalWeb LANGUAGES CXX)
find_package(AxiomRuntime CONFIG REQUIRED)
add_executable(external_web main.cpp)
if(NOT AXIOM_NLOHMANN_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/json/nlohmann/json.hpp")
  set(AXIOM_NLOHMANN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/json")
endif()
if(NOT AXIOM_NLOHMANN_INCLUDE_DIR OR NOT EXISTS "${AXIOM_NLOHMANN_INCLUDE_DIR}/nlohmann/json.hpp")
  message(FATAL_ERROR "Set AXIOM_NLOHMANN_INCLUDE_DIR to the application's nlohmann/json include directory")
endif()
target_include_directories(external_web PRIVATE "${AXIOM_NLOHMANN_INCLUDE_DIR}")
target_link_libraries(external_web PRIVATE Axiom::PlatformWeb)
target_compile_features(external_web PRIVATE cxx_std_20)
target_link_options(external_web PRIVATE "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
  "-sALLOW_MEMORY_GROWTH=1" "-sEXPORTED_FUNCTIONS=['_main','_sdk_input','_sdk_logical_width','_sdk_logical_height','_sdk_schedule','_sdk_object_count','_sdk_selection_count','_sdk_selection_diagnostics','_sdk_connector_preset','_sdk_connector_count_by_routing','_sdk_hover','_sdk_camera_scale','_sdk_view','_sdk_active_count','_sdk_resize','_sdk_control','_sdk_import_image','_sdk_image_selected','_sdk_crop_active','_sdk_crop_scale','_sdk_crop_zoom','_sdk_crop','_sdk_section','_sdk_ink_tool','_sdk_shutdown','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToNewUTF8','stringToUTF16','HEAPU8']"
  "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/axiom-font.otf@/axiom-font.otf\""
  "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/fonts@/fonts\"")
set_target_properties(external_web PROPERTIES SUFFIX ".js")
add_custom_command(TARGET external_web POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/index.html" "$<TARGET_FILE_DIR:external_web>/index.html"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/pointer_input.mjs" "$<TARGET_FILE_DIR:external_web>/pointer_input.mjs"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/text_input.mjs" "$<TARGET_FILE_DIR:external_web>/text_input.mjs"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/serve.py" "$<TARGET_FILE_DIR:external_web>/serve.py")
