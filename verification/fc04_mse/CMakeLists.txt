add_executable(axiom_fc04_mse_tests mse_01_27_test.cpp)
target_link_libraries(axiom_fc04_mse_tests PRIVATE canvas_runtime_composition GTest::gtest_main)
target_compile_definitions(axiom_fc04_mse_tests PRIVATE AXIOM_RUNTIME_QUALIFICATION=1 MSE_REPO_ROOT="${PROJECT_SOURCE_DIR}")
target_compile_features(axiom_fc04_mse_tests PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(axiom_fc04_mse_tests PRIVATE /W4 /permissive- /WX /EHsc)
else()
  target_compile_options(axiom_fc04_mse_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
foreach(number RANGE 1 27)
  if(number LESS 10)
    set(id "0${number}")
  else()
    set(id "${number}")
  endif()
  add_test(NAME FC04.MSE-${id}
    COMMAND axiom_fc04_mse_tests --gtest_filter=MultiSelectionMSE.MSE_${id}
            --gtest_fail_if_no_test_selected)
endforeach()
add_test(NAME SDKCppPublicMultiPermissionStale
  COMMAND axiom_fc04_mse_tests --gtest_filter=MultiSelectionMSE.PublicFacadePermissionChangeInvalidatesQueuedWrite
          --gtest_fail_if_no_test_selected)
if(WIN32 AND EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
  add_custom_command(TARGET axiom_fc04_mse_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_fc04_mse_tests>/icudtl.dat")
endif()

if(NOT EMSCRIPTEN)
  add_executable(axiom_fc04_mse_fault_probe mse_fault_probe.cpp)
  target_link_libraries(axiom_fc04_mse_fault_probe PRIVATE canvas_runtime_composition)
  target_compile_definitions(axiom_fc04_mse_fault_probe PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
  target_compile_features(axiom_fc04_mse_fault_probe PRIVATE cxx_std_20)
  add_dependencies(axiom_fc04_mse_tests axiom_fc04_mse_fault_probe)
  target_compile_definitions(axiom_fc04_mse_tests PRIVATE
      AXIOM_MSE_FAULT_CTEST_PATH="${CMAKE_CTEST_COMMAND}"
      AXIOM_MSE_FAULT_BUILD_DIR="${CMAKE_BINARY_DIR}")
  if(MSVC)
    target_compile_options(axiom_fc04_mse_fault_probe PRIVATE /W4 /permissive- /WX /EHsc)
  else()
    target_compile_options(axiom_fc04_mse_fault_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME FC04MSEHistoryFaultUndo COMMAND axiom_fc04_mse_fault_probe undo)
  add_test(NAME FC04MSEHistoryFaultRedo COMMAND axiom_fc04_mse_fault_probe redo)
  add_test(NAME FC04MSEMultiIntent COMMAND axiom_fc04_mse_fault_probe intent)
  add_test(NAME FC04MSEMultiIntentFault COMMAND axiom_fc04_mse_fault_probe intent-fault)
  add_test(NAME FC04MSEForwardFaultSemantic COMMAND axiom_fc04_mse_fault_probe forward-semantic)
  add_test(NAME FC04MSEForwardFaultScene COMMAND axiom_fc04_mse_fault_probe forward-scene)
  set_tests_properties(FC04MSEHistoryFaultUndo FC04MSEHistoryFaultRedo
      FC04MSEMultiIntent FC04MSEMultiIntentFault FC04MSEForwardFaultSemantic
      FC04MSEForwardFaultScene PROPERTIES TIMEOUT 20)
  if(WIN32 AND EXISTS "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat")
    add_custom_command(TARGET axiom_fc04_mse_fault_probe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CANVAS_SKIA_SDK_ROOT}/runtime/windows/icudtl.dat" "$<TARGET_FILE_DIR:axiom_fc04_mse_fault_probe>/icudtl.dat")
  endif()
endif()
