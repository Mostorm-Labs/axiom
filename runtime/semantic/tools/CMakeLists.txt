add_executable(canvas_semantic_golden_probe golden_probe.cpp)
target_link_libraries(canvas_semantic_golden_probe PRIVATE canvas_runtime_semantic)

if(MSVC)
  target_compile_options(canvas_semantic_golden_probe PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_golden_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(canvas_semantic_g1_07_replay_inspector
  g1_07_replay_inspector_main.cpp
  g1_07_replay_inspector.cpp
  g1_07_replay_corpus.cpp
  g1_06_projection.cpp
  g1_06_digest.cpp)
target_link_libraries(canvas_semantic_g1_07_replay_inspector PRIVATE canvas_runtime_semantic)
target_include_directories(canvas_semantic_g1_07_replay_inspector PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
if(MSVC)
  target_compile_options(canvas_semantic_g1_07_replay_inspector PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_07_replay_inspector PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(canvas_semantic_g1_08_verifier
  g1_08_verifier_main.cpp
  g1_08_verifier.cpp
  g1_08_locality_workload.cpp)
target_link_libraries(canvas_semantic_g1_08_verifier PRIVATE canvas_runtime_semantic)
target_include_directories(canvas_semantic_g1_08_verifier PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
  target_compile_options(canvas_semantic_g1_08_verifier PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(canvas_semantic_g1_08_verifier PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
