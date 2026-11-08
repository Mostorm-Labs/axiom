add_library(canvas_runtime_foundation INTERFACE)
target_include_directories(canvas_runtime_foundation INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # RuntimeFacade's Image value types consume the public Semantic headers.
    # This is a header dependency, not a dependency on the Semantic library.
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include)
target_compile_features(canvas_runtime_foundation INTERFACE cxx_std_20)

# Collector objects are private and never installed. Normal Runtime remains unchanged.
if(AXIOM_G5_UR0_DIAGNOSTICS)
  add_library(axiom_g5_observation STATIC src/g5_observation.cpp)
  target_include_directories(axiom_g5_observation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(axiom_g5_observation PRIVATE cxx_std_20)
  target_link_libraries(canvas_runtime_foundation INTERFACE axiom_g5_observation)
endif()
if(BUILD_TESTING AND AXIOM_BUILD_RUNTIME_QUALIFICATION AND NOT EMSCRIPTEN)
  add_executable(axiom_g5_observation_tests tests/g5_observation_test.cpp src/g5_observation.cpp)
  target_compile_definitions(axiom_g5_observation_tests PRIVATE AXIOM_G5_UR0_DIAGNOSTICS=1)
  target_include_directories(axiom_g5_observation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/runtime/resources/include ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
  target_compile_features(axiom_g5_observation_tests PRIVATE cxx_std_20)
  add_test(NAME G5Ur0Observation COMMAND axiom_g5_observation_tests)
endif()
