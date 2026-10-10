add_library(axiom_ink_playground_common STATIC
    common/ink_playground_host.cpp
    common/canvas_control_binding.cpp
    common/canvas_runtime_facade_adapter.cpp
    common/text_qualification_resources.cpp
    common/brush_commit_adapter.cpp
    common/platform_brush_baseline_observation.cpp
    common/platform_contract.cpp
    common/platform_interaction_controller.cpp
    platform/apple/apple_input_adapter.cpp)
target_include_directories(axiom_ink_playground_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(axiom_ink_playground_common PUBLIC
    canvas_runtime_foundation canvas_runtime_control canvas_runtime_input canvas_runtime_ink
    canvas_runtime_interaction canvas_runtime_render)
target_compile_features(axiom_ink_playground_common PUBLIC cxx_std_20)

# Resolve the published SDK in this directory as well as runtime/render: the
# imported target there is directory-scoped, and qualification uses its headers.
if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
  if(CANVAS_SKIA_SDK_ROOT AND NOT TARGET CanvasSkia::Skia)
    list(APPEND CMAKE_PREFIX_PATH "${CANVAS_SKIA_SDK_ROOT}")
    find_package(CanvasSkia CONFIG REQUIRED
      PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  endif()
endif()

if(TARGET CanvasSkia::Skia)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(G47_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/g47")
  foreach(pair IN ITEMS "latin|${CanvasSkia_FONT_PATH}|kG47LatinFont" "cjk|${CanvasSkia_CJK_FONT_PATH}|kG47CjkFont")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 name)
    list(GET fields 1 font)
    list(GET fields 2 symbol)
    add_custom_command(OUTPUT "${G47_RESOURCE_DIR}/g47_${name}_font.hpp"
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/runtime/render/tools/embed_binary.py"
        "${G47_RESOURCE_DIR}/g47_${name}_font.hpp" "${symbol}" "${font}"
      DEPENDS "${font}" "${PROJECT_SOURCE_DIR}/runtime/render/tools/embed_binary.py")
    target_sources(axiom_ink_playground_common PRIVATE "${G47_RESOURCE_DIR}/g47_${name}_font.hpp")
  endforeach()
  target_include_directories(axiom_ink_playground_common PRIVATE "${G47_RESOURCE_DIR}")
  target_compile_definitions(axiom_ink_playground_common PRIVATE AXIOM_G47_BUNDLED_RESOURCES=1)
endif()

if(MSVC)
  target_compile_options(axiom_ink_playground_common PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_ink_playground_common PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
  add_executable(axiom_ink_playground_tests tests/ink_playground_test.cpp)
  target_link_libraries(axiom_ink_playground_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_ink_playground_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_ink_playground_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_ink_playground_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME G4InkPlaygroundComposition COMMAND axiom_ink_playground_tests composition)
  add_test(NAME G4InkPlaygroundHud COMMAND axiom_ink_playground_tests hud)
  add_test(NAME G4InkPlaygroundSurface COMMAND axiom_ink_playground_tests surface)
  add_test(NAME G4InkPlaygroundPreview COMMAND axiom_ink_playground_tests preview)
  add_test(NAME G4InkPlaygroundRepeatedStrokes COMMAND axiom_ink_playground_tests repeated)
  add_test(NAME G4InkPlaygroundPlatform COMMAND axiom_ink_playground_tests platform)
  add_test(NAME G4MultiPointerPlayground COMMAND axiom_ink_playground_tests multipointer)
  add_test(NAME G4ProvisionalZeroMutation COMMAND axiom_ink_playground_tests provisional)
  add_test(NAME G4ViewportGestureState COMMAND axiom_ink_playground_tests viewport)
  add_test(NAME G46BViewportFitHost COMMAND axiom_ink_playground_tests viewport-fit)
  add_executable(axiom_dui_m1_tool_tests tests/dui_m1_tool_options_test.cpp)
  target_link_libraries(axiom_dui_m1_tool_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_dui_m1_tool_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_dui_m1_tool_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  endif()
  add_test(NAME DUIM1ToolOptions COMMAND axiom_dui_m1_tool_tests)
  add_executable(axiom_dui_m1_view_tests tests/dui_m1_view_controls_test.cpp)
  target_link_libraries(axiom_dui_m1_view_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_dui_m1_view_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_dui_m1_view_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  endif()
  add_test(NAME DUIM1ViewControls COMMAND axiom_dui_m1_view_tests)
  add_executable(axiom_dui_m1_platform_tests tests/dui_m1_platform_control_test.cpp)
  target_link_libraries(axiom_dui_m1_platform_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_dui_m1_platform_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_dui_m1_platform_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  endif()
  add_test(NAME DUIM1PlatformControl COMMAND axiom_dui_m1_platform_tests)
  add_executable(axiom_dui_m1_qualification_runner tests/dui_m1_qualification_runner.cpp)
  target_link_libraries(axiom_dui_m1_qualification_runner PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_dui_m1_qualification_runner PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_dui_m1_qualification_runner PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  endif()
  add_test(NAME DUIM1Qualification COMMAND axiom_dui_m1_qualification_runner
    "${CMAKE_BINARY_DIR}/dui-m1-qualification")
  add_executable(axiom_g46b_query_structure_tests tests/viewport_query_structure_test.cpp)
  target_link_libraries(axiom_g46b_query_structure_tests PRIVATE axiom_ink_playground_common)
  target_compile_definitions(axiom_g46b_query_structure_tests PRIVATE
    AXIOM_INK_PLAYGROUND_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_features(axiom_g46b_query_structure_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_g46b_query_structure_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_g46b_query_structure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME G46BQueryStructure COMMAND axiom_g46b_query_structure_tests)
  add_test(NAME G4AndroidPointerIdentity COMMAND axiom_ink_playground_tests android-pointer-identity)
  add_test(NAME G4MultiContactPolicySelection COMMAND axiom_ink_playground_tests multi-contact-policy)
  add_test(NAME G4InkPlaygroundSelection COMMAND axiom_ink_playground_tests selection)
  if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
    add_executable(axiom_playground_transform_tests tests/selection_transform_test.cpp)
    target_link_libraries(axiom_playground_transform_tests PRIVATE axiom_ink_playground_common)
    target_compile_features(axiom_playground_transform_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_playground_transform_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_playground_transform_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME G46PlaygroundTransform COMMAND axiom_playground_transform_tests)
  endif()
  add_executable(axiom_platform_brush_baseline_runner tests/platform_brush_baseline_runner.cpp)
  target_link_libraries(axiom_platform_brush_baseline_runner PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_platform_brush_baseline_runner PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_platform_brush_baseline_runner PRIVATE /W4 /permissive- /WX /EHsc)
  else()
    target_compile_options(axiom_platform_brush_baseline_runner PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if((CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
      AND CANVAS_SEMANTIC_ENABLE_PROTOBUF)
  add_executable(axiom_g47_text_host_tests tests/g47_text_host_test.cpp)
  target_link_libraries(axiom_g47_text_host_tests PRIVATE axiom_ink_playground_common)
  target_compile_definitions(axiom_g47_text_host_tests PRIVATE
    G47_LATIN_FONT="${CanvasSkia_FONT_PATH}" G47_CJK_FONT="${CanvasSkia_CJK_FONT_PATH}")
  if(MSVC)
    target_compile_options(axiom_g47_text_host_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(axiom_g47_text_host_tests PRIVATE -UNDEBUG)
  endif()
  if(WIN32 AND CanvasSkia_ICU_DATA_PATH)
    add_custom_command(TARGET axiom_g47_text_host_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CanvasSkia_ICU_DATA_PATH}"
        "$<TARGET_FILE_DIR:axiom_g47_text_host_tests>/icudtl.dat")
  endif()
  add_test(NAME G47TextHost COMMAND axiom_g47_text_host_tests)
  add_executable(axiom_g47_qualification_runner tests/g47_qualification_runner.cpp)
  target_link_libraries(axiom_g47_qualification_runner PRIVATE axiom_ink_playground_common)
  if(WIN32 AND CanvasSkia_ICU_DATA_PATH)
    add_custom_command(TARGET axiom_g47_qualification_runner POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CanvasSkia_ICU_DATA_PATH}"
        "$<TARGET_FILE_DIR:axiom_g47_qualification_runner>/icudtl.dat")
  endif()
  add_test(NAME G47QualificationReplay COMMAND axiom_g47_qualification_runner "${CMAKE_BINARY_DIR}/g47-qualification" 32)
  add_executable(axiom_g46c_qualification_runner tests/g46c_qualification_runner.cpp)
  target_link_libraries(axiom_g46c_qualification_runner PRIVATE axiom_ink_playground_common)
  if(TARGET CanvasSkia::Skia)
    target_link_libraries(axiom_g46c_qualification_runner PRIVATE CanvasSkia::Skia)
  endif()
  target_compile_features(axiom_g46c_qualification_runner PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_g46c_qualification_runner PRIVATE /W4 /permissive- /WX /EHsc)
  else()
    target_compile_options(axiom_g46c_qualification_runner PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME G46CQualificationBundle
    COMMAND axiom_g46c_qualification_runner
      "${CMAKE_BINARY_DIR}/g46c-qualification" "$<CONFIG>")
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME G46CQualificationBehavior
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/g46c_qualification_behavior_test.py"
      "$<TARGET_FILE:axiom_g46c_qualification_runner>")
  else()
    message(STATUS "G4.6-C qualification unavailable: requires published Skia and Semantic codec profiles")
  endif()
  add_executable(axiom_brush_runtime_pipeline_tests tests/brush_runtime_pipeline_test.cpp)
  target_link_libraries(axiom_brush_runtime_pipeline_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_brush_runtime_pipeline_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_brush_runtime_pipeline_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_brush_runtime_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME G45BrushRuntimePipeline COMMAND axiom_brush_runtime_pipeline_tests)
  add_executable(axiom_brush_commit_adapter_tests tests/brush_commit_adapter_test.cpp)
  target_link_libraries(axiom_brush_commit_adapter_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_brush_commit_adapter_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_brush_commit_adapter_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_brush_commit_adapter_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME G45BrushCommitAdapter COMMAND axiom_brush_commit_adapter_tests)
  add_executable(axiom_c1_apple_input_contract_tests tests/c1_apple_input_contract_test.cpp)
  target_link_libraries(axiom_c1_apple_input_contract_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_c1_apple_input_contract_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_c1_apple_input_contract_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_c1_apple_input_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME C1AppleContract COMMAND axiom_c1_apple_input_contract_tests)
  if(WIN32 AND TARGET Arc::Windows)
    add_executable(axiom_ink_playground_windows_pointer_utils_tests
      tests/windows_pointer_utils_test.cpp
      platform/windows/windows_smoke_evidence.cpp)
    target_compile_features(axiom_ink_playground_windows_pointer_utils_tests PRIVATE cxx_std_20)
    target_link_libraries(axiom_ink_playground_windows_pointer_utils_tests PRIVATE
      canvas_runtime_ink canvas_runtime_interaction Arc::Windows)
    if(MSVC)
      target_compile_options(axiom_ink_playground_windows_pointer_utils_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_ink_playground_windows_pointer_utils_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4InkPlaygroundWindowsPointerUtils
      COMMAND axiom_ink_playground_windows_pointer_utils_tests)
  endif()
endif()

if(WIN32 AND TARGET arc_Windows)
  if(CANVAS_SKIA_SDK_ROOT AND NOT TARGET CanvasSkia::Skia)
    list(APPEND CMAKE_PREFIX_PATH "${CANVAS_SKIA_SDK_ROOT}")
    find_package(CanvasSkia CONFIG REQUIRED
      PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  endif()
  add_executable(axiom_ink_playground_windows WIN32
    platform/windows/main.cpp
    platform/windows/windows_d3d12_skia_surface_provider.cpp
    platform/windows/windows_smoke_evidence.cpp)
  if(CanvasSkia_ICU_DATA_PATH)
    add_custom_command(TARGET axiom_ink_playground_windows POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CanvasSkia_ICU_DATA_PATH}"
        "$<TARGET_FILE_DIR:axiom_ink_playground_windows>/icudtl.dat")
  endif()
  target_include_directories(axiom_ink_playground_windows PRIVATE
      ${CMAKE_SOURCE_DIR}/debug_ui/include)
  target_link_libraries(axiom_ink_playground_windows PRIVATE
      axiom_ink_playground_common canvas_debug_ui Arc::Windows CanvasSkia::Skia
      d3d12 dxgi dcomp libcpmt)
  if(BUILD_TESTING)
    add_executable(axiom_windows_preview_dispatch_tests
      tests/windows_preview_dispatch_test.cpp
      platform/windows/windows_d3d12_skia_surface_provider.cpp
      platform/windows/windows_smoke_evidence.cpp)
    target_include_directories(axiom_windows_preview_dispatch_tests PRIVATE
      ${CMAKE_SOURCE_DIR}/debug_ui/include)
    target_compile_features(axiom_windows_preview_dispatch_tests PRIVATE cxx_std_20)
    target_link_libraries(axiom_windows_preview_dispatch_tests PRIVATE
      axiom_ink_playground_common canvas_debug_ui Arc::Windows CanvasSkia::Skia
      d3d12 dxgi dcomp libcpmt)
    if(MSVC)
      target_compile_options(axiom_windows_preview_dispatch_tests PRIVATE /UNDEBUG)
    endif()
    add_test(NAME G4WindowsPreviewDispatch COMMAND axiom_windows_preview_dispatch_tests)
    add_executable(axiom_ink_playground_windows_preview_popup_tests
      tests/windows_preview_popup_test.cpp
      platform/windows/windows_d3d12_skia_surface_provider.cpp)
    target_compile_features(axiom_ink_playground_windows_preview_popup_tests PRIVATE cxx_std_20)
    target_link_libraries(axiom_ink_playground_windows_preview_popup_tests PRIVATE
      axiom_ink_playground_common Arc::Windows CanvasSkia::Skia d3d12 dxgi dcomp libcpmt)
    add_test(NAME G4InkPlaygroundWindowsPreviewPopup
      COMMAND axiom_ink_playground_windows_preview_popup_tests)
  endif()
endif()

if(ANDROID AND TARGET arc_Android)
  if(CANVAS_SKIA_SDK_ROOT AND NOT TARGET CanvasSkia::Skia)
    list(APPEND CMAKE_PREFIX_PATH "${CANVAS_SKIA_SDK_ROOT}")
    find_package(CanvasSkia CONFIG REQUIRED
      PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  endif()
  add_library(axiom_ink_playground_android SHARED
      platform/android/bridge.cpp
      platform/android/android_jni.cpp
      platform/android/android_egl_surface_provider.cpp)
  target_link_libraries(axiom_ink_playground_android PRIVATE
      axiom_ink_playground_common Arc::Android CanvasSkia::Skia)
  target_link_options(axiom_ink_playground_android PRIVATE
      "-Wl,-z,max-page-size=16384"
      "-Wl,-z,common-page-size=16384")
endif()

if(EMSCRIPTEN)
  add_executable(axiom_ink_playground_web platform/web/bridge.cpp)
  target_link_libraries(axiom_ink_playground_web PRIVATE axiom_ink_playground_common)
  target_include_directories(axiom_ink_playground_web PRIVATE ${CANVAS_SKIA_SDK_ROOT})
  if(AXIOM_BUILD_DEBUG_UI)
    target_link_libraries(axiom_ink_playground_web PRIVATE canvas_debug_ui)
    target_include_directories(axiom_ink_playground_web PRIVATE ${CMAKE_SOURCE_DIR}/.deps/imgui)
    target_compile_definitions(axiom_ink_playground_web PRIVATE AXIOM_WEB_DEBUG_UI=1)
  else()
    target_compile_definitions(axiom_ink_playground_web PRIVATE AXIOM_WEB_DEBUG_UI=0)
  endif()
  set_target_properties(axiom_ink_playground_web PROPERTIES SUFFIX ".js")
  configure_file(platform/web/index.html
      ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
  configure_file(platform/web/evidence_worker.js
      ${CMAKE_CURRENT_BINARY_DIR}/evidence_worker.js COPYONLY)
  configure_file(tests/web_preview_lifecycle_test.html
      ${CMAKE_CURRENT_BINARY_DIR}/web_preview_lifecycle_test.html COPYONLY)
  target_link_options(axiom_ink_playground_web PRIVATE
      "-sEXPORTED_FUNCTIONS+=['_axiom_ink_text_scenario','_axiom_ink_text_action','_axiom_ink_text_metrics']"
      "-sMODULARIZE=1" "-sEXPORT_ES6=1"
      "-sEXPORT_NAME=createAxiomInkPlayground"
      "-sENVIRONMENT=web" "-sALLOW_MEMORY_GROWTH=1" "-sUSE_WEBGL2=1"
      "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToNewUTF8']"
      "-sEXPORTED_FUNCTIONS=['_malloc','_free','_axiom_ink_create','_axiom_ink_destroy','_axiom_ink_platform_batch','_axiom_ink_select_tool','_axiom_ink_select_brush_profile','_axiom_ink_eraser_begin','_axiom_ink_eraser_sample','_axiom_ink_eraser_finish','_axiom_ink_eraser_cancel','_axiom_ink_brush_render','_axiom_ink_brush_size','_axiom_ink_brush_opacity','_axiom_ink_brush_representation','_axiom_ink_render_submission_count','_axiom_ink_render_flush_count','_axiom_ink_run_baseline_observation','_axiom_ink_viewport_claimed','_axiom_ink_viewport_scale','_axiom_ink_viewport_translation_x','_axiom_ink_viewport_translation_y','_axiom_ink_apply_viewport_wheel_pan','_axiom_ink_apply_viewport_ctrl_wheel_zoom','_axiom_ink_apply_viewport_gesture','_axiom_ink_bind_surface','_axiom_ink_bind_preview_surface','_axiom_ink_resize_surface','_axiom_ink_lose_surface','_axiom_ink_rebind_surface','_axiom_ink_point_count','_axiom_ink_point_x','_axiom_ink_point_y','_axiom_ink_selected_tool','_axiom_ink_debug_ui_init','_axiom_ink_debug_ui_toggle','_axiom_ink_debug_ui_visible','_axiom_ink_debug_ui_wants_capture_mouse','_axiom_ink_debug_ui_wants_capture_keyboard','_axiom_ink_debug_ui_render','_axiom_ink_debug_ui_pointer','_axiom_ink_debug_ui_key','_axiom_ink_debug_ui_capture_begin','_axiom_ink_debug_ui_capture_route','_axiom_ink_debug_ui_capture_terminal','_axiom_ink_debug_ui_resize']")
endif()
  add_executable(axiom_dui_m1_runtime_facade_tests tests/dui_m1_runtime_facade_test.cpp)
  target_link_libraries(axiom_dui_m1_runtime_facade_tests PRIVATE axiom_ink_playground_common)
  target_compile_features(axiom_dui_m1_runtime_facade_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_dui_m1_runtime_facade_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  endif()
  add_test(NAME DUIM1RuntimeFacade COMMAND axiom_dui_m1_runtime_facade_tests)
