# Local functional-test consumer only. Raw strict failures remain FAIL.
# Do not change the global warning policy or any repository source.
function(axiom_local_baseline_diagnostic)
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/runtime/semantic/src/apply_plan.cpp"
    TARGET_DIRECTORY canvas_runtime_semantic
    PROPERTIES COMPILE_OPTIONS "-Wno-error=dangling-else")
endfunction()
cmake_language(DEFER CALL axiom_local_baseline_diagnostic)
