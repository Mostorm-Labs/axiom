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
  set(catalog_root "${PROJECT_SOURCE_DIR}/assets/fonts/imported")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${catalog_root}/catalog.json")
  file(READ "${catalog_root}/catalog.json" catalog)
  string(JSON count LENGTH "${catalog}" fonts)
  set(AXIOM_IMPORTED_FONT_COUNT "${count}")
  set(AXIOM_IMPORTED_FONT_ENTRIES "")
  math(EXPR last "${count} - 1")
  foreach(index RANGE ${last})
    foreach(field file sha256 bytes resource_id_uint64 license_file)
      string(JSON ${field} GET "${catalog}" fonts ${index} ${field})
    endforeach()
    file(SHA256 "${catalog_root}/${file}" actual_hash)
    file(SIZE "${catalog_root}/${file}" actual_length)
    if(NOT actual_hash STREQUAL sha256 OR NOT actual_length EQUAL bytes OR
        NOT EXISTS "${catalog_root}/${license_file}")
      message(FATAL_ERROR "Imported application font identity/license mismatch: ${file}")
    endif()
    string(APPEND AXIOM_IMPORTED_FONT_ENTRIES
      "    {${resource_id_uint64}ULL, \"${file}\", \"${sha256}\", ${bytes}ULL},\n")
  endforeach()
  set(AXIOM_HOST_FONT_CATALOG_ROOT "${catalog_root}" PARENT_SCOPE)
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
  # All native hosts share one output directory. Serialize asset packaging;
  # simultaneous POST_BUILD directory copies can race on Windows file handles.
  if(NOT TARGET axiom_host_text_font_assets)
    set(destination "${CMAKE_CURRENT_BINARY_DIR}")
    add_custom_target(axiom_host_text_font_assets
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${AXIOM_HOST_FONT_CATALOG_ROOT}" "${destination}/fonts"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_PATH}" "${destination}/axiom-font.otf"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_ROOT}/LICENSE.txt" "${destination}/axiom-font.LICENSE.txt"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AXIOM_HOST_FONT_ROOT}/NOTICE.txt" "${destination}/axiom-font.NOTICE.txt")
  endif()
  add_dependencies(${target} axiom_host_text_font_assets)
endfunction()
