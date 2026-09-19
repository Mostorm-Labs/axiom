add_library(canvas_runtime_interaction STATIC
    src/editor_session.cpp
    src/interaction_runtime.cpp
    src/session_manager.cpp)

target_include_directories(canvas_runtime_interaction PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(canvas_runtime_interaction PUBLIC canvas_runtime_foundation)
target_compile_features(canvas_runtime_interaction PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(canvas_runtime_interaction PRIVATE /W4 /permissive- /WX /EHsc)
else()
    target_compile_options(canvas_runtime_interaction PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(axiom_interaction_session_tests
        tests/session_manager_test.cpp)
    target_link_libraries(axiom_interaction_session_tests PRIVATE canvas_runtime_interaction)
    target_compile_features(axiom_interaction_session_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_interaction_session_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_interaction_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4InteractionSessionManager COMMAND axiom_interaction_session_tests)
endif()
