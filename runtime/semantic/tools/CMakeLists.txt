add_executable(canvas_semantic_golden_probe golden_probe.cpp)
target_link_libraries(canvas_semantic_golden_probe PRIVATE canvas_runtime_semantic)

if(MSVC)
  target_compile_options(canvas_semantic_golden_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_golden_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
