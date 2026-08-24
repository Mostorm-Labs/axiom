add_subdirectory(hooks)
add_subdirectory(common)
add_subdirectory(windows)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
