add_library(axiom_ink_playground_common STATIC
    common/ink_playground_host.cpp
    common/platform_contract.cpp)
target_include_directories(axiom_ink_playground_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(axiom_ink_playground_common PUBLIC
    canvas_runtime_foundation canvas_runtime_input canvas_runtime_ink
    canvas_runtime_interaction canvas_runtime_render)
target_compile_features(axiom_ink_playground_common PUBLIC cxx_std_20)

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
    target_compile_options(axiom_ink_playground_tests PRIVATE /W4 /permissive- /WX /EHsc)
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
  if(WIN32)
    add_executable(axiom_ink_playground_windows_pointer_utils_tests
      tests/windows_pointer_utils_test.cpp
      platform/windows/windows_smoke_evidence.cpp)
    target_compile_features(axiom_ink_playground_windows_pointer_utils_tests PRIVATE cxx_std_20)
    target_link_libraries(axiom_ink_playground_windows_pointer_utils_tests PRIVATE
      canvas_runtime_ink)
    if(MSVC)
      target_compile_options(axiom_ink_playground_windows_pointer_utils_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
      target_compile_options(axiom_ink_playground_windows_pointer_utils_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4InkPlaygroundWindowsPointerUtils
      COMMAND axiom_ink_playground_windows_pointer_utils_tests)
  endif()
endif()

if(WIN32 AND TARGET arc_Windows)
  add_executable(axiom_ink_playground_windows WIN32
    platform/windows/main.cpp
    platform/windows/windows_smoke_evidence.cpp)
  target_link_libraries(axiom_ink_playground_windows PRIVATE
      axiom_ink_playground_common Arc::Windows)
endif()

if(ANDROID AND TARGET arc_Android)
  add_library(axiom_ink_playground_android SHARED
      platform/android/bridge.cpp
      platform/android/android_jni.cpp)
  target_link_libraries(axiom_ink_playground_android PRIVATE
      axiom_ink_playground_common Arc::Android)
  target_link_options(axiom_ink_playground_android PRIVATE
      "-Wl,-z,max-page-size=16384"
      "-Wl,-z,common-page-size=16384")
endif()

if(EMSCRIPTEN)
  add_executable(axiom_ink_playground_web platform/web/bridge.cpp)
  target_link_libraries(axiom_ink_playground_web PRIVATE axiom_ink_playground_common)
  set_target_properties(axiom_ink_playground_web PROPERTIES SUFFIX ".js")
  configure_file(platform/web/index.html
      ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
  target_link_options(axiom_ink_playground_web PRIVATE
      "-sMODULARIZE=1" "-sEXPORT_ES6=1"
      "-sEXPORT_NAME=createAxiomInkPlayground"
      "-sENVIRONMENT=web" "-sALLOW_MEMORY_GROWTH=1"
      "-sEXPORTED_FUNCTIONS=['_malloc','_free','_axiom_ink_create','_axiom_ink_destroy','_axiom_ink_begin','_axiom_ink_batch','_axiom_ink_sample','_axiom_ink_commit','_axiom_ink_begin_pointer','_axiom_ink_pointer_sample','_axiom_ink_commit_pointer','_axiom_ink_bind_surface','_axiom_ink_resize_surface','_axiom_ink_lose_surface','_axiom_ink_point_count','_axiom_ink_point_x','_axiom_ink_point_y']")
endif()
