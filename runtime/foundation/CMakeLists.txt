add_library(canvas_runtime_foundation INTERFACE)
target_include_directories(canvas_runtime_foundation INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # RuntimeFacade's Image value types consume the public Semantic headers.
    # This is a header dependency, not a dependency on the Semantic library.
    ${PROJECT_SOURCE_DIR}/runtime/semantic/include)
target_compile_features(canvas_runtime_foundation INTERFACE cxx_std_20)
