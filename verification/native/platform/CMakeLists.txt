add_subdirectory(hooks)
add_subdirectory(common)
add_subdirectory(windows)
add_subdirectory(android)
add_subdirectory(apple)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
