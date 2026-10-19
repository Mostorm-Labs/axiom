add_library(axiom_canvas_demo_common STATIC
    common/camera_controller.cpp
    common/demo_hit_select.cpp
    platform/windows/windows_surface_adapter.cpp
    platform/web/web_surface_host.cpp)
target_include_directories(axiom_canvas_demo_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(axiom_canvas_demo_common PUBLIC
    canvas_runtime_foundation canvas_runtime_scene canvas_runtime_render
    canvas_runtime_composition)
target_compile_features(axiom_canvas_demo_common PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(axiom_canvas_demo_common PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(axiom_canvas_demo_common PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32)
    add_executable(axiom_canvas_demo_windows WIN32
        platform/windows/main.cpp
        platform/windows/windows_canonical_surface_host.cpp)
    target_link_libraries(axiom_canvas_demo_windows PRIVATE
        axiom_canvas_demo_common)
    target_include_directories(axiom_canvas_demo_windows PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
    target_compile_features(axiom_canvas_demo_windows PRIVATE cxx_std_20)
    target_compile_definitions(axiom_canvas_demo_windows PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(axiom_canvas_demo_windows PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_canvas_demo_windows PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_link_libraries(axiom_canvas_demo_windows PRIVATE user32)
endif()

if(CANVAS_RENDER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(axiom_demo_camera_tests tests/camera_controller_test.cpp)
    target_link_libraries(axiom_demo_camera_tests PRIVATE axiom_canvas_demo_common)
    target_compile_features(axiom_demo_camera_tests PRIVATE cxx_std_20)
    add_test(NAME G3Camera COMMAND axiom_demo_camera_tests)

    add_executable(axiom_demo_hit_select_tests tests/demo_hit_select_test.cpp)
    target_link_libraries(axiom_demo_hit_select_tests PRIVATE axiom_canvas_demo_common)
    target_compile_features(axiom_demo_hit_select_tests PRIVATE cxx_std_20)
    add_test(NAME G3DemoHitSelect COMMAND axiom_demo_hit_select_tests)

    foreach(target axiom_demo_camera_tests axiom_demo_hit_select_tests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
        endif()
    endforeach()

    add_executable(axiom_demo_windows_surface_tests
        tests/windows_surface_adapter_test.cpp)
    target_link_libraries(axiom_demo_windows_surface_tests PRIVATE
        axiom_canvas_demo_common)
    target_include_directories(axiom_demo_windows_surface_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
    target_compile_features(axiom_demo_windows_surface_tests PRIVATE cxx_std_20)
    add_test(NAME G3WindowsSurfaceAdapter COMMAND axiom_demo_windows_surface_tests)

    add_executable(axiom_demo_web_surface_tests
        tests/web_surface_host_test.cpp)
    target_link_libraries(axiom_demo_web_surface_tests PRIVATE
        axiom_canvas_demo_common)
    target_include_directories(axiom_demo_web_surface_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/web)
    target_compile_features(axiom_demo_web_surface_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_demo_web_surface_tests PRIVATE /W4 /permissive- /WX /EHsc /UNDEBUG)
    else()
        target_compile_options(axiom_demo_web_surface_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    add_test(NAME G3WebSurfaceHost COMMAND axiom_demo_web_surface_tests)
endif()

if(EMSCRIPTEN)
    include(${PROJECT_SOURCE_DIR}/cmake/AxiomHostTextFont.cmake)
    axiom_resolve_host_text_font()
    add_executable(axiom_canvas_demo_web
        platform/web/bridge.cpp)
    target_link_libraries(axiom_canvas_demo_web PRIVATE axiom_canvas_demo_common)
    target_include_directories(axiom_canvas_demo_web PRIVATE platform/web ${AXIOM_HOST_FONT_INCLUDE} ${PROJECT_SOURCE_DIR}/apps/minimal_runtime_host ${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include)
    find_package(CanvasSkia CONFIG REQUIRED PATHS "${CANVAS_SKIA_SDK_ROOT}/lib/cmake/CanvasSkia" NO_DEFAULT_PATH)
    target_compile_features(axiom_canvas_demo_web PRIVATE cxx_std_20)
    axiom_configure_build_info(axiom_canvas_demo_web)
    if(AXIOM_BUILD_DEBUG_UI)
        target_link_libraries(axiom_canvas_demo_web PRIVATE canvas_debug_ui)
        target_include_directories(axiom_canvas_demo_web PRIVATE
            ${CMAKE_SOURCE_DIR}/.deps/imgui)
        target_include_directories(axiom_canvas_demo_web SYSTEM PRIVATE ${CANVAS_SKIA_SDK_ROOT})
        target_compile_definitions(axiom_canvas_demo_web PRIVATE AXIOM_WEB_DEBUG_UI=1)
    else()
        target_compile_definitions(axiom_canvas_demo_web PRIVATE AXIOM_WEB_DEBUG_UI=0)
    endif()
    target_compile_options(axiom_canvas_demo_web PRIVATE -Wall -Wextra -Wpedantic -Werror)
    set_target_properties(axiom_canvas_demo_web PROPERTIES SUFFIX ".js")
    configure_file(platform/web/index.html
        ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
    configure_file(platform/web/text_input.js ${CMAKE_CURRENT_BINARY_DIR}/text_input.js COPYONLY)
    configure_file(platform/web/host.js ${CMAKE_CURRENT_BINARY_DIR}/host.js COPYONLY)
    configure_file(tests/web_fc02_text_test.html ${CMAKE_CURRENT_BINARY_DIR}/tests/web_fc02_text_test.html COPYONLY)
    configure_file(tests/web_minimal_runtime_test.html
        ${CMAKE_CURRENT_BINARY_DIR}/tests/web_minimal_runtime_test.html COPYONLY)
    configure_file(${AXIOM_HOST_FONT_ROOT}/LICENSE.txt ${CMAKE_CURRENT_BINARY_DIR}/axiom-font.LICENSE.txt COPYONLY)
    configure_file(${AXIOM_HOST_FONT_ROOT}/NOTICE.txt ${CMAKE_CURRENT_BINARY_DIR}/axiom-font.NOTICE.txt COPYONLY)
    target_link_options(axiom_canvas_demo_web PRIVATE
        "SHELL:--preload-file \"${AXIOM_HOST_FONT_PATH}@/axiom-font.otf\""
        "SHELL:--preload-file \"${AXIOM_HOST_FONT_ROOT}/LICENSE.txt@/axiom-font.LICENSE.txt\""
        "SHELL:--preload-file \"${AXIOM_HOST_FONT_ROOT}/NOTICE.txt@/axiom-font.NOTICE.txt\""
        "-sMODULARIZE=1" "-sEXPORT_ES6=1" "-sEXPORT_NAME=createAxiomCanvasModule"
        "-sENVIRONMENT=web" "-sALLOW_MEMORY_GROWTH=1" "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
        "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
        "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToNewUTF8','stringToUTF16']")
endif()
