# Private same-build sidecar. Published single-threaded Skia archives only.
find_package(CanvasSkia CONFIG REQUIRED PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
set(_cpu_identity "${CMAKE_CXX_COMPILER}:${CMAKE_CXX_COMPILER_VERSION}:${CMAKE_CXX_FLAGS}:${CMAKE_BUILD_TYPE}:cpu-v1:heap67108864:stack1048576:input1048576:output1048576:queue8:transport16777216:steps8:us2000")
foreach(_source
 "r1-full-skia-sdk.lock.json" "sdk/web/cpu-worker.cmake"
 "runtime/render/src/cpu_exact_path_kernel.cpp" "runtime/render/src/cpu_exact_path_kernel.hpp"
 "runtime/render/src/cpu_worker_protocol.cpp" "runtime/render/src/cpu_worker_protocol.hpp"
 "runtime/render/src/cpu_worker_bridge.cpp" "runtime/semantic/include/canvas/semantic/erase_mask.hpp"
 "runtime/semantic/include/canvas/semantic/semantic_geometry.hpp")
 file(SHA256 "${PROJECT_SOURCE_DIR}/${_source}" _hash)
 string(APPEND _cpu_identity ":${_source}:${_hash}")
 set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${_source}")
endforeach()
file(SHA256 "${CANVAS_SKIA_SDK_ROOT}/manifest.json" _cpu_sdk)
file(SHA256 "${CMAKE_TOOLCHAIN_FILE}" _cpu_toolchain)
string(SHA256 _cpu_build "${_cpu_identity}:${_cpu_sdk}:${_cpu_toolchain}")
string(SUBSTRING "${_cpu_build}" 0 32 AXIOM_CPU_KERNEL_ID)
target_compile_definitions(canvas_runtime_render PRIVATE AXIOM_CPU_KERNEL_ID="${AXIOM_CPU_KERNEL_ID}")
add_executable(axiom_cpu_worker_kernel
 "${PROJECT_SOURCE_DIR}/runtime/render/src/cpu_exact_path_kernel.cpp"
 "${PROJECT_SOURCE_DIR}/runtime/render/src/cpu_worker_protocol.cpp"
 "${PROJECT_SOURCE_DIR}/runtime/render/src/cpu_worker_bridge.cpp")
target_include_directories(axiom_cpu_worker_kernel PRIVATE
 "${PROJECT_SOURCE_DIR}/runtime/semantic/include" "${PROJECT_SOURCE_DIR}/runtime/foundation/include")
target_compile_features(axiom_cpu_worker_kernel PRIVATE cxx_std_20)
target_compile_options(axiom_cpu_worker_kernel PRIVATE -fexceptions -fno-rtti)
target_compile_definitions(axiom_cpu_worker_kernel PRIVATE AXIOM_CPU_KERNEL_ID="${AXIOM_CPU_KERNEL_ID}")
target_link_libraries(axiom_cpu_worker_kernel PRIVATE CanvasSkia::Skia)
target_link_options(axiom_cpu_worker_kernel PRIVATE -fexceptions --no-entry
 -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker -sFILESYSTEM=0
 -sINITIAL_MEMORY=67108864 -sMAXIMUM_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=0
 -sSTACK_SIZE=1048576 -sMALLOC=emmalloc -sABORTING_MALLOC=0
 "-sEXPORTED_FUNCTIONS=['_malloc','_free','_axiom_cpu_build','_axiom_cpu_create','_axiom_cpu_step','_axiom_cpu_cursor','_axiom_cpu_max_union_ns','_axiom_cpu_result','_axiom_cpu_result_size','_axiom_cpu_cancel','_axiom_cpu_release','_axiom_cpu_validate']"
 "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8']")
set_target_properties(axiom_cpu_worker_kernel PROPERTIES OUTPUT_NAME cpu-worker-kernel SUFFIX .mjs
 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/internal")
add_dependencies(axiom_js_runtime axiom_cpu_worker_kernel)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-build.mjs"
 "export const kernelId='${AXIOM_CPU_KERNEL_ID}';\nexport const limits=Object.freeze({heapBytes:67108864,queueJobs:8,inputBytes:1048576,outputBytes:1048576,transportBytes:16777216,stepSegments:8,stepMicroseconds:2000});\n")
foreach(name client thread owner)
 configure_file("${CMAKE_CURRENT_SOURCE_DIR}/internal/cpu-worker-${name}.mjs"
   "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-${name}.mjs" COPYONLY)
endforeach()
install(FILES
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-kernel.mjs"
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-kernel.wasm"
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-build.mjs"
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-client.mjs"
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-thread.mjs"
 "${CMAKE_CURRENT_BINARY_DIR}/internal/cpu-worker-owner.mjs" DESTINATION js/internal)
