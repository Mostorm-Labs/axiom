add_library(canvas_runtime_interaction STATIC
    src/eraser_session.cpp
    src/editor_session.cpp
    src/hit_test_service.cpp
    src/interaction_runtime.cpp
    src/selection_session.cpp
    src/transform_session.cpp
    src/session_manager.cpp
    src/editor_history.cpp)

target_include_directories(canvas_runtime_interaction PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(canvas_runtime_interaction PUBLIC canvas_runtime_foundation canvas_runtime_semantic)
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

    add_executable(axiom_selection_session_tests
        tests/selection_session_test.cpp)
    target_link_libraries(axiom_selection_session_tests PRIVATE canvas_runtime_interaction)
    target_compile_features(axiom_selection_session_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_selection_session_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_selection_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4SelectionSession COMMAND axiom_selection_session_tests)
    add_executable(axiom_transform_session_tests tests/transform_session_test.cpp)
    target_link_libraries(axiom_transform_session_tests PRIVATE canvas_runtime_interaction)
    target_compile_features(axiom_transform_session_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_transform_session_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_transform_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4TransformSession COMMAND axiom_transform_session_tests)
    add_executable(axiom_eraser_session_tests tests/eraser_session_test.cpp)
    target_link_libraries(axiom_eraser_session_tests PRIVATE canvas_runtime_interaction)
    target_compile_features(axiom_eraser_session_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_eraser_session_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_eraser_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4EraserSession COMMAND axiom_eraser_session_tests)

    add_executable(axiom_editor_history_tests tests/editor_history_test.cpp)
    target_link_libraries(axiom_editor_history_tests PRIVATE canvas_runtime_interaction)
    target_compile_features(axiom_editor_history_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(axiom_editor_history_tests PRIVATE /W4 /permissive- /WX /EHsc)
    else()
        target_compile_options(axiom_editor_history_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME G4EditorHistory COMMAND axiom_editor_history_tests)
endif()
