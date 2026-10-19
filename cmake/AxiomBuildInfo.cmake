function(axiom_configure_build_info target)
  execute_process(COMMAND git -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
                  OUTPUT_VARIABLE AXIOM_BUILD_SOURCE_REVISION
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT AXIOM_BUILD_SOURCE_REVISION)
    set(AXIOM_BUILD_SOURCE_REVISION "unknown")
  endif()
  execute_process(COMMAND git -C "${PROJECT_SOURCE_DIR}" diff --quiet
                  RESULT_VARIABLE AXIOM_BUILD_GIT_CLEAN ERROR_QUIET)
  if(AXIOM_BUILD_GIT_CLEAN EQUAL 0)
    set(AXIOM_BUILD_DIRTY 0)
  else()
    set(AXIOM_BUILD_DIRTY 1)
  endif()
  if(WIN32)
    set(AXIOM_BUILD_PLATFORM "windows")
  elseif(ANDROID)
    set(AXIOM_BUILD_PLATFORM "android")
  elseif(EMSCRIPTEN)
    set(AXIOM_BUILD_PLATFORM "web")
  else()
    set(AXIOM_BUILD_PLATFORM "host")
  endif()
  target_compile_definitions(${target} PRIVATE
    AXIOM_BUILD_SOURCE_REVISION="${AXIOM_BUILD_SOURCE_REVISION}"
    AXIOM_BUILD_CONFIGURATION="$<CONFIG>"
    AXIOM_BUILD_PLATFORM="${AXIOM_BUILD_PLATFORM}"
    AXIOM_BUILD_VERSION="${PROJECT_VERSION}"
    AXIOM_BUILD_DIRTY=${AXIOM_BUILD_DIRTY})
endfunction()
