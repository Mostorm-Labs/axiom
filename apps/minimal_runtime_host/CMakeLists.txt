include(${PROJECT_SOURCE_DIR}/cmake/AxiomHostTextFont.cmake)
axiom_resolve_host_text_font()

add_library(axiom_minimal_runtime_host STATIC minimal_runtime_host.cpp minimal_runtime_facade.cpp)
target_include_directories(axiom_minimal_runtime_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${AXIOM_HOST_FONT_INCLUDE})
target_include_directories(axiom_minimal_runtime_host PUBLIC
  ${PROJECT_SOURCE_DIR}/runtime/control/include
  ${PROJECT_SOURCE_DIR}/runtime/foundation/include)
target_link_libraries(axiom_minimal_runtime_host PUBLIC
  canvas_runtime_composition canvas_runtime_control)
target_compile_features(axiom_minimal_runtime_host PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(axiom_minimal_runtime_host PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_minimal_runtime_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32 AND (CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH))
  # Imported package targets are directory-scoped. Resolve the accepted SDK in
  # this consumer too; a sibling Render directory does not make them visible.
  if(NOT TARGET CanvasSkia::Skia)
    find_package(CanvasSkia CONFIG REQUIRED
      PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  endif()
  add_library(axiom_minimal_windows_surface STATIC
    windows_d3d12_surface_provider.cpp windows_runtime_adapter.cpp)
  if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded")
    install(TARGETS axiom_minimal_windows_surface ARCHIVE DESTINATION lib)
    install(FILES "${PROJECT_SOURCE_DIR}/runtime/composition/include/canvas/platform/windows_runtime_adapter.hpp"
      DESTINATION include/canvas/platform)
  endif()
  target_include_directories(axiom_minimal_windows_surface PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/runtime/render/include
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include
    ${PROJECT_SOURCE_DIR}/runtime/scene/include
    ${CANVAS_SKIA_SDK_ROOT})
  target_link_libraries(axiom_minimal_windows_surface PUBLIC d3d12 dxgi dcomp imm32)
  target_link_libraries(axiom_minimal_windows_surface PUBLIC canvas_runtime_composition)
  if(TARGET CanvasSkia::Skia)
    target_link_libraries(axiom_minimal_windows_surface PUBLIC CanvasSkia::Skia)
  endif()
  target_compile_features(axiom_minimal_windows_surface PUBLIC cxx_std_20)
  target_compile_definitions(axiom_minimal_windows_surface PUBLIC CANVAS_RENDER_HAS_SKIA=1)
  target_include_directories(axiom_minimal_windows_surface PUBLIC
    ${PROJECT_SOURCE_DIR}/runtime/interaction/include
    ${PROJECT_SOURCE_DIR}/runtime/input/include)
  if(MSVC)
    target_compile_options(axiom_minimal_windows_surface PRIVATE /W4 /permissive- /WX /EHsc /wd4201 /wd4244 /wd4267)
  endif()
  add_executable(axiom_minimal_runtime_host_windows WIN32
    windows_main.cpp)
  target_link_libraries(axiom_minimal_runtime_host_windows PRIVATE
    axiom_minimal_runtime_host axiom_minimal_windows_surface canvas_runtime_control user32 gdi32 imm32 libcpmt)
  if(AXIOM_BUILD_DEBUG_UI)
    target_link_libraries(axiom_minimal_runtime_host_windows PRIVATE canvas_debug_ui)
    target_link_libraries(axiom_minimal_runtime_host_windows PRIVATE comdlg32)
    target_compile_definitions(axiom_minimal_runtime_host_windows PRIVATE AXIOM_MINIMAL_HAS_DEBUG_UI=1)
  else()
    target_compile_definitions(axiom_minimal_runtime_host_windows PRIVATE AXIOM_MINIMAL_HAS_DEBUG_UI=0)
  endif()

  if(TARGET CanvasSkia::Skia)
    target_link_libraries(axiom_minimal_runtime_host_windows PRIVATE CanvasSkia::Skia)
  endif()
  target_include_directories(axiom_minimal_runtime_host_windows PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/runtime/control/include
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include
    ${CANVAS_SKIA_SDK_ROOT})
  target_compile_features(axiom_minimal_runtime_host_windows PRIVATE cxx_std_20)
  axiom_configure_build_info(axiom_minimal_runtime_host_windows)
  axiom_package_host_text_font(axiom_minimal_runtime_host_windows)
  # Package variables are directory-scoped; the imported Skia target may have
  # been created by a sibling consumer. Use the locked package's runtime asset.
  if(EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
    add_custom_command(TARGET axiom_minimal_runtime_host_windows POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat"
        "$<TARGET_FILE_DIR:axiom_minimal_runtime_host_windows>/icudtl.dat")
  endif()
  target_compile_definitions(axiom_minimal_runtime_host_windows PRIVATE
    NOMINMAX)
  if(MSVC)
    target_compile_options(axiom_minimal_runtime_host_windows PRIVATE /W4 /permissive- /WX /EHsc /wd4201 /wd4244 /wd4267)
  else()
    target_compile_options(axiom_minimal_runtime_host_windows PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

if(BUILD_TESTING)
  if(AXIOM_BUILD_DEBUG_UI)
    add_executable(axiom_minimal_section_debug_ui_tests tests/section_debug_ui_test.cpp)
    target_link_libraries(axiom_minimal_section_debug_ui_tests PRIVATE axiom_minimal_runtime_host canvas_debug_ui)
    target_include_directories(axiom_minimal_section_debug_ui_tests PRIVATE ${PROJECT_SOURCE_DIR}/.deps/imgui)
    target_compile_features(axiom_minimal_section_debug_ui_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_minimal_section_debug_ui_tests PRIVATE /W4 /WX /EHsc /UNDEBUG)
    else()
      target_compile_options(axiom_minimal_section_debug_ui_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    if(WIN32 AND EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
      add_custom_command(TARGET axiom_minimal_section_debug_ui_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_minimal_section_debug_ui_tests>/icudtl.dat")
    endif()
    add_test(NAME FC08MinimalSectionDebugUi COMMAND axiom_minimal_section_debug_ui_tests)
  endif()
  add_executable(axiom_fc02_text_consumer_tests tests/text_consumer_test.cpp)
  target_link_libraries(axiom_fc02_text_consumer_tests PRIVATE axiom_minimal_runtime_host)
  target_compile_definitions(axiom_fc02_text_consumer_tests PRIVATE FC02_HOST_FONT="${AXIOM_HOST_FONT_PATH}")
  target_compile_features(axiom_fc02_text_consumer_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_fc02_text_consumer_tests PRIVATE /W4 /WX /EHsc)
  else()
    target_compile_options(axiom_fc02_text_consumer_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(WIN32 AND EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
    add_custom_command(TARGET axiom_fc02_text_consumer_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_fc02_text_consumer_tests>/icudtl.dat")
  endif()
  add_test(NAME FC02MinimalTextConsumer COMMAND axiom_fc02_text_consumer_tests)
  if(WIN32 AND TARGET axiom_minimal_windows_surface)
    if(AXIOM_BUILD_DEBUG_UI)
      add_executable(axiom_minimal_windows_geometry_fit_tests tests/windows_geometry_fit_test.cpp)
      target_link_libraries(axiom_minimal_windows_geometry_fit_tests PRIVATE axiom_minimal_runtime_host axiom_minimal_windows_surface canvas_debug_ui user32 gdi32 libcpmt)
      target_include_directories(axiom_minimal_windows_geometry_fit_tests PRIVATE ${PROJECT_SOURCE_DIR}/.deps/imgui)
      target_compile_features(axiom_minimal_windows_geometry_fit_tests PRIVATE cxx_std_20)
      target_compile_definitions(axiom_minimal_windows_geometry_fit_tests PRIVATE NOMINMAX)
      target_compile_options(axiom_minimal_windows_geometry_fit_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
      axiom_package_host_text_font(axiom_minimal_windows_geometry_fit_tests)
      add_custom_command(TARGET axiom_minimal_windows_geometry_fit_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_minimal_windows_geometry_fit_tests>/icudtl.dat")
      add_test(NAME FC05WindowsFractionalSticky COMMAND axiom_minimal_windows_geometry_fit_tests sticky)
      add_test(NAME FC05WindowsAttachmentFit COMMAND axiom_minimal_windows_geometry_fit_tests fit)
      set_tests_properties(FC05WindowsFractionalSticky FC05WindowsAttachmentFit PROPERTIES SKIP_RETURN_CODE 77)
      add_executable(axiom_minimal_windows_image_debug_ui_tests tests/windows_image_debug_ui_test.cpp)
      target_link_libraries(axiom_minimal_windows_image_debug_ui_tests PRIVATE axiom_minimal_runtime_host canvas_debug_ui comdlg32 shlwapi)
      target_include_directories(axiom_minimal_windows_image_debug_ui_tests PRIVATE ${PROJECT_SOURCE_DIR}/.deps/imgui)
      target_compile_definitions(axiom_minimal_windows_image_debug_ui_tests PRIVATE FC03_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/verification/fc03/fixtures")
      target_compile_options(axiom_minimal_windows_image_debug_ui_tests PRIVATE /W4 /WX /EHsc /UNDEBUG)
      add_custom_command(TARGET axiom_minimal_windows_image_debug_ui_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_minimal_windows_image_debug_ui_tests>/icudtl.dat")
      add_test(NAME FC03WindowsImageDebugUi COMMAND axiom_minimal_windows_image_debug_ui_tests)
      add_executable(axiom_minimal_windows_text_tool_entry_tests tests/windows_text_tool_entry_test.cpp)
      target_link_libraries(axiom_minimal_windows_text_tool_entry_tests PRIVATE axiom_minimal_runtime_host axiom_minimal_windows_surface canvas_debug_ui user32 gdi32 libcpmt)
      target_include_directories(axiom_minimal_windows_text_tool_entry_tests PRIVATE ${PROJECT_SOURCE_DIR}/.deps/imgui)
      target_compile_features(axiom_minimal_windows_text_tool_entry_tests PRIVATE cxx_std_20)
      target_compile_definitions(axiom_minimal_windows_text_tool_entry_tests PRIVATE NOMINMAX AXIOM_MINIMAL_HAS_DEBUG_UI=1 AXIOM_RUNTIME_QUALIFICATION=1)
      if(MSVC)
        target_compile_options(axiom_minimal_windows_text_tool_entry_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG /wd4201 /wd4244 /wd4267)
      endif()
      axiom_package_host_text_font(axiom_minimal_windows_text_tool_entry_tests)
      add_custom_command(TARGET axiom_minimal_windows_text_tool_entry_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_minimal_windows_text_tool_entry_tests>/icudtl.dat")
      add_test(NAME FC02WindowsTextToolEntry COMMAND axiom_minimal_windows_text_tool_entry_tests entry)
      add_test(NAME FC02WindowsTextToolFocus COMMAND axiom_minimal_windows_text_tool_entry_tests focus)
      add_test(NAME FC02WindowsTextToolTouchFocus COMMAND axiom_minimal_windows_text_tool_entry_tests touch)
      add_test(NAME FC02WindowsTextToolAfterInk COMMAND axiom_minimal_windows_text_tool_entry_tests after-ink)
      add_test(NAME FC02WindowsTextFormatControls COMMAND axiom_minimal_windows_text_tool_entry_tests format)
      add_test(NAME FC02WindowsInkDoubleClick COMMAND axiom_minimal_windows_text_tool_entry_tests ink-double-click)
      add_test(NAME FC02WindowsShapeLabelEntry COMMAND axiom_minimal_windows_text_tool_entry_tests shape-label)
      add_test(NAME FC02WindowsTouchShapeLabelEntry COMMAND axiom_minimal_windows_text_tool_entry_tests touch-shape-label)
      add_test(NAME FC04WindowsGroupDoubleClick COMMAND axiom_minimal_windows_text_tool_entry_tests group-dblclick)
      add_test(NAME FC04WindowsGroupTouchDoubleTap COMMAND axiom_minimal_windows_text_tool_entry_tests group-touch)
      add_test(NAME SDKWindowsHostPublicAdapter COMMAND axiom_minimal_windows_text_tool_entry_tests public-adapter)
      set_tests_properties(SDKWindowsHostPublicAdapter PROPERTIES SKIP_RETURN_CODE 77)
    endif()
    add_executable(axiom_minimal_windows_text_pipeline_tests tests/windows_text_pipeline_test.cpp)
    target_link_libraries(axiom_minimal_windows_text_pipeline_tests PRIVATE axiom_minimal_runtime_host axiom_minimal_windows_surface user32 gdi32 libcpmt)
    target_compile_features(axiom_minimal_windows_text_pipeline_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_minimal_windows_text_pipeline_tests PRIVATE NOMINMAX AXIOM_MINIMAL_HAS_DEBUG_UI=0 AXIOM_RUNTIME_QUALIFICATION=1)
    if(MSVC)
      target_compile_options(axiom_minimal_windows_text_pipeline_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG /wd4201 /wd4244 /wd4267)
    endif()
    axiom_package_host_text_font(axiom_minimal_windows_text_pipeline_tests)
    add_custom_command(TARGET axiom_minimal_windows_text_pipeline_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_minimal_windows_text_pipeline_tests>/icudtl.dat")
    add_test(NAME FC02WindowsTextIngress COMMAND axiom_minimal_windows_text_pipeline_tests)
    add_executable(axiom_minimal_windows_touch_pipeline_tests
      tests/windows_touch_pipeline_test.cpp)
    target_link_libraries(axiom_minimal_windows_touch_pipeline_tests PRIVATE
      axiom_minimal_runtime_host axiom_minimal_windows_surface user32 gdi32 libcpmt)
    target_compile_features(axiom_minimal_windows_touch_pipeline_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_minimal_windows_touch_pipeline_tests PRIVATE
      NOMINMAX AXIOM_MINIMAL_HAS_DEBUG_UI=0 AXIOM_RUNTIME_QUALIFICATION=1)
    if(MSVC)
      target_compile_options(axiom_minimal_windows_touch_pipeline_tests PRIVATE
        /W4 /permissive- /WX /EHsc /UNDEBUG /wd4201 /wd4244 /wd4267)
    endif()
    add_test(NAME FC00XWindowsTouchIngress COMMAND axiom_minimal_windows_touch_pipeline_tests)
    add_executable(axiom_minimal_windows_input_pipeline_tests
      tests/windows_input_pipeline_test.cpp)
    target_link_libraries(axiom_minimal_windows_input_pipeline_tests PRIVATE
      axiom_minimal_runtime_host axiom_minimal_windows_surface user32 gdi32 libcpmt)
    target_compile_features(axiom_minimal_windows_input_pipeline_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_minimal_windows_input_pipeline_tests PRIVATE
      NOMINMAX AXIOM_MINIMAL_HAS_DEBUG_UI=0 AXIOM_RUNTIME_QUALIFICATION=1)
    if(MSVC)
      target_compile_options(axiom_minimal_windows_input_pipeline_tests PRIVATE
        /W4 /permissive- /WX /EHsc /UNDEBUG /wd4201 /wd4244 /wd4267)
    endif()
    add_test(NAME FC00XWindowsMouseHistory
      COMMAND axiom_minimal_windows_input_pipeline_tests history)
    add_test(NAME FC01AWindowsSelectionInput COMMAND axiom_minimal_windows_input_pipeline_tests selection)
    add_test(NAME FC00XWindowsPreviewOrigin
      COMMAND axiom_minimal_windows_input_pipeline_tests origin)
    add_test(NAME FC00XPreviewCanonicalCoordinates
      COMMAND axiom_minimal_windows_input_pipeline_tests coordinates)
    add_executable(axiom_minimal_windows_surface_binding_tests
      tests/windows_surface_binding_test.cpp)
    target_link_libraries(axiom_minimal_windows_surface_binding_tests PRIVATE
      axiom_minimal_runtime_host axiom_minimal_windows_surface user32 libcpmt)
    target_compile_features(axiom_minimal_windows_surface_binding_tests PRIVATE cxx_std_20)
    target_compile_definitions(axiom_minimal_windows_surface_binding_tests PRIVATE NOMINMAX)
    if(MSVC)
      target_compile_options(axiom_minimal_windows_surface_binding_tests PRIVATE
        /W4 /permissive- /WX /EHsc /UNDEBUG /wd4201 /wd4244 /wd4267)
    endif()
    add_test(NAME FC00XS2WindowsSurfaceBinding COMMAND axiom_minimal_windows_surface_binding_tests)
    set_tests_properties(FC00XS2WindowsSurfaceBinding PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(axiom_windows_public_adapter_tests tests/windows_public_adapter_test.cpp)
    target_link_libraries(axiom_windows_public_adapter_tests PRIVATE axiom_minimal_windows_surface canvas_runtime_composition)
    target_compile_features(axiom_windows_public_adapter_tests PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(axiom_windows_public_adapter_tests PRIVATE /W4 /WX /EHsc /UNDEBUG)
    endif()
    add_test(NAME SDKWindowsPublicAdapter COMMAND axiom_windows_public_adapter_tests)
    set_tests_properties(SDKWindowsPublicAdapter PROPERTIES SKIP_RETURN_CODE 77)
  endif()
  add_executable(axiom_minimal_runtime_host_tests tests/minimal_runtime_host_test.cpp)
  target_link_libraries(axiom_minimal_runtime_host_tests PRIVATE axiom_minimal_runtime_host)
  target_compile_definitions(axiom_minimal_runtime_host_tests PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
  target_compile_features(axiom_minimal_runtime_host_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_minimal_runtime_host_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_minimal_runtime_host_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
  endif()
  add_test(NAME FC00XMinimalRuntimeHost COMMAND axiom_minimal_runtime_host_tests)

  add_executable(axiom_minimal_presentation_cache_tests
    tests/presentation_cache_test.cpp)
  target_include_directories(axiom_minimal_presentation_cache_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(axiom_minimal_presentation_cache_tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(axiom_minimal_presentation_cache_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
  else()
    target_compile_options(axiom_minimal_presentation_cache_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
  endif()
  add_test(NAME FC00XMinimalPresentationCache
    COMMAND axiom_minimal_presentation_cache_tests)
endif()
