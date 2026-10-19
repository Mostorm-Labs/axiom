add_library(canvas_runtime_text STATIC src/text_layout.cpp src/sha256.cpp)
target_include_directories(canvas_runtime_text PUBLIC include)
target_link_libraries(canvas_runtime_text PUBLIC canvas_runtime_foundation canvas_runtime_semantic)
target_compile_features(canvas_runtime_text PUBLIC cxx_std_20)
if(CANVAS_RENDER_ENABLE_SKIA_HEADLESS OR CANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH)
  find_package(CanvasSkia CONFIG REQUIRED
    PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  target_link_libraries(canvas_runtime_text PRIVATE CanvasSkia::Paragraph)
  target_compile_definitions(canvas_runtime_text PRIVATE CANVAS_TEXT_HAS_SKIA=1)
  if(BUILD_TESTING)
    add_executable(axiom_text_layout_tests tests/text_layout_test.cpp)
    target_link_libraries(axiom_text_layout_tests PRIVATE canvas_runtime_text canvas_runtime_scene)
    target_compile_definitions(axiom_text_layout_tests PRIVATE
      G47_LATIN_FONT="${CanvasSkia_FONT_PATH}" G47_CJK_FONT="${CanvasSkia_CJK_FONT_PATH}")
    if(MSVC)
      target_compile_options(axiom_text_layout_tests PRIVATE /UNDEBUG)
    else()
      target_compile_options(axiom_text_layout_tests PRIVATE -UNDEBUG)
    endif()
    if(WIN32 AND CanvasSkia_ICU_DATA_PATH)
      add_custom_command(TARGET axiom_text_layout_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CanvasSkia_ICU_DATA_PATH}"
                "$<TARGET_FILE_DIR:axiom_text_layout_tests>/icudtl.dat")
    endif()
    add_test(NAME G47TextLayout COMMAND axiom_text_layout_tests)
  endif()
endif()
