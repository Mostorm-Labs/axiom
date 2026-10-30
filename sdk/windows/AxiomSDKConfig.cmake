get_filename_component(_axiom_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Axiom SDK consumer requires Windows x64")
endif()
if(NOT TARGET Axiom::SDK)
  add_library(Axiom::SDK SHARED IMPORTED)
  set_target_properties(Axiom::SDK PROPERTIES
    IMPORTED_IMPLIB "${_axiom_prefix}/lib/axiom.lib"
    IMPORTED_LOCATION "${_axiom_prefix}/bin/axiom.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${_axiom_prefix}/include"
    INTERFACE_COMPILE_FEATURES cxx_std_20)
endif()
set(_AXIOM_SDK_PREFIX "${_axiom_prefix}")
function(axiom_deploy target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_AXIOM_SDK_PREFIX}/bin/axiom.dll" "$<TARGET_FILE_DIR:${target}>/axiom.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${_AXIOM_SDK_PREFIX}/bin/share" "$<TARGET_FILE_DIR:${target}>/share"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_AXIOM_SDK_PREFIX}/bin/share/axiom/icudtl.dat" "$<TARGET_FILE_DIR:${target}>/icudtl.dat")
endfunction()
