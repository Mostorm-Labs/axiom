# Pass as CMAKE_PROJECT_INCLUDE only for the SDK-01 verification build.
if(PROJECT_NAME STREQUAL "canvas_v2_pocs" AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/verification/sdk01/owner-tests.cmake")
endif()
