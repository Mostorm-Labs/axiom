add_library(axiom_minimal_runtime_host STATIC minimal_runtime_host.cpp minimal_runtime_facade.cpp)
target_include_directories(axiom_minimal_runtime_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
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

if(WIN32)
  add_library(axiom_minimal_windows_surface STATIC
    windows_d3d12_surface_provider.cpp)
  target_include_directories(axiom_minimal_windows_surface PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/runtime/render/include
    ${PROJECT_SOURCE_DIR}/runtime/foundation/include
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include
    ${PROJECT_SOURCE_DIR}/runtime/scene/include
    ${CANVAS_SKIA_SDK_ROOT})
  target_link_libraries(axiom_minimal_windows_surface PUBLIC d3d12 dxgi dcomp)
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
    axiom_minimal_runtime_host axiom_minimal_windows_surface canvas_runtime_control user32 gdi32 libcpmt)
  if(AXIOM_BUILD_DEBUG_UI)
    target_link_libraries(axiom_minimal_runtime_host_windows PRIVATE canvas_debug_ui)
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
  target_compile_definitions(axiom_minimal_runtime_host_windows PRIVATE
    NOMINMAX)
  if(MSVC)
    target_compile_options(axiom_minimal_runtime_host_windows PRIVATE /W4 /permissive- /WX /EHsc /wd4201 /wd4244 /wd4267)
  else()
    target_compile_options(axiom_minimal_runtime_host_windows PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(axiom_minimal_runtime_host_tests tests/minimal_runtime_host_test.cpp)
  target_link_libraries(axiom_minimal_runtime_host_tests PRIVATE axiom_minimal_runtime_host)
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
