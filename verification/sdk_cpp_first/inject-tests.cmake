# Verification-only injection for a real composition owner; not an installed SDK target.
if(PROJECT_NAME STREQUAL "canvas_v2_pocs" AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/verification/sdk01/owner-tests.cmake")
  cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/verification/sdk_cpp_first/tests.cmake")
endif()
