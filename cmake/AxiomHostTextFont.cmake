include_guard(GLOBAL)

# Application resource consumption only; no font or SDK compilation/download.
function(axiom_resolve_host_text_font)
  set(root "${PROJECT_SOURCE_DIR}/assets/fonts/noto-sans-cjk-sc")
  file(READ "${root}/resource.json" manifest)
  string(JSON name GET "${manifest}" file)
  string(JSON expected GET "${manifest}" sha256)
  string(JSON expected_size GET "${manifest}" bytes)
  set(path "${root}/${name}")
  file(SHA256 "${path}" actual)
  file(SIZE "${path}" actual_size)
  if(NOT actual STREQUAL expected OR NOT actual_size EQUAL expected_size)
    message(FATAL_ERROR "Application text font does not match pinned resource.json")
  endif()
  set(AXIOM_HOST_FONT_SHA256 "${expected}")
  set(generated "${PROJECT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${generated}/axiom")
  configure_file("${PROJECT_SOURCE_DIR}/apps/minimal_runtime_host/host_text_font.hpp.in"
                 "${generated}/axiom/host_text_font.hpp" @ONLY)
  set(AXIOM_HOST_FONT_PATH "${path}" PARENT_SCOPE)
  set(AXIOM_HOST_FONT_ROOT "${root}" PARENT_SCOPE)
  set(AXIOM_HOST_FONT_INCLUDE "${generated}" PARENT_SCOPE)
endfunction()

function(axiom_package_host_text_font target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_PATH}" "$<TARGET_FILE_DIR:${target}>/axiom-font.otf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_ROOT}/LICENSE.txt" "$<TARGET_FILE_DIR:${target}>/axiom-font.LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_ROOT}/NOTICE.txt" "$<TARGET_FILE_DIR:${target}>/axiom-font.NOTICE.txt")
endfunction()
