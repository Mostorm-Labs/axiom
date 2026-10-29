add_executable(axiom_sdk01_owner_mixed_stroke_probe
  "${CMAKE_SOURCE_DIR}/verification/sdk01/owner_mixed_stroke_probe.cpp")
target_link_libraries(axiom_sdk01_owner_mixed_stroke_probe PRIVATE canvas_runtime_composition)
target_compile_definitions(axiom_sdk01_owner_mixed_stroke_probe PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
target_compile_features(axiom_sdk01_owner_mixed_stroke_probe PRIVATE cxx_std_20)
if(WIN32 AND EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
  add_custom_command(TARGET axiom_sdk01_owner_mixed_stroke_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat"
      "$<TARGET_FILE_DIR:axiom_sdk01_owner_mixed_stroke_probe>/icudtl.dat")
endif()
if(MSVC)
  target_compile_options(axiom_sdk01_owner_mixed_stroke_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_sdk01_owner_mixed_stroke_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
# A diagnostic control, not a claim that the required leaf-patch API exists.
add_test(NAME SDK01WholeStrokeReplacementControl
  COMMAND axiom_sdk01_owner_mixed_stroke_probe --observe-replacement)
