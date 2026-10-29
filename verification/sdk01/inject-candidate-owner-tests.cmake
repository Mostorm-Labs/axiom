# Verification-only injection; preserve the package-bound owner test file.
if(PROJECT_NAME STREQUAL "canvas_v2_pocs" AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/verification/sdk01/owner-tests.cmake")
  cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/verification/sdk01/candidate-owner-tests.cmake")
endif()
