# Internal build policy. Limits are ceilings, never preallocated memory.
if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME MATCHES "^(Android|iOS)$")
    set(_axiom_render_default_profile CONSERVATIVE)
else()
    set(_axiom_render_default_profile DESKTOP)
endif()
set(AXIOM_RENDER_MEMORY_PROFILE "${_axiom_render_default_profile}" CACHE STRING
    "Internal render working set: DESKTOP or CONSERVATIVE")
set_property(CACHE AXIOM_RENDER_MEMORY_PROFILE PROPERTY STRINGS DESKTOP CONSERVATIVE)
if(AXIOM_RENDER_MEMORY_PROFILE STREQUAL "DESKTOP")
    set(_axiom_render_defaults 256 512 64)
elseif(AXIOM_RENDER_MEMORY_PROFILE STREQUAL "CONSERVATIVE")
    set(_axiom_render_defaults 32 96 8)
else()
    message(FATAL_ERROR "AXIOM_RENDER_MEMORY_PROFILE must be DESKTOP or CONSERVATIVE")
endif()
list(GET _axiom_render_defaults 0 _axiom_render_cache)
list(GET _axiom_render_defaults 1 _axiom_render_shared)
list(GET _axiom_render_defaults 2 _axiom_render_entry)
set(AXIOM_RENDER_CACHE_MIB "${_axiom_render_cache}" CACHE STRING "Optional derived render cache MiB")
set(AXIOM_RENDER_SHARED_MIB "${_axiom_render_shared}" CACHE STRING "Shared derived resource hard cap MiB")
# Preserve older configured 32/96 builds which have no entry override yet.
# An explicit entry value is never clamped silently.
if(AXIOM_RENDER_CACHE_MIB MATCHES "^[1-9][0-9]*$" AND
   AXIOM_RENDER_CACHE_MIB LESS _axiom_render_entry)
    set(_axiom_render_entry "${AXIOM_RENDER_CACHE_MIB}")
endif()
set(AXIOM_RENDER_ENTRY_MIB "${_axiom_render_entry}" CACHE STRING "Maximum individual derived cache entry MiB")
foreach(_axiom_render_budget AXIOM_RENDER_CACHE_MIB AXIOM_RENDER_SHARED_MIB AXIOM_RENDER_ENTRY_MIB)
    if(NOT "${${_axiom_render_budget}}" MATCHES "^[1-9][0-9]*$" OR
       "${${_axiom_render_budget}}" GREATER 4096)
        message(FATAL_ERROR "${_axiom_render_budget} must be an integer from 1 to 4096")
    endif()
endforeach()
if(AXIOM_RENDER_ENTRY_MIB GREATER AXIOM_RENDER_CACHE_MIB OR
   AXIOM_RENDER_CACHE_MIB GREATER AXIOM_RENDER_SHARED_MIB)
    message(FATAL_ERROR "Render limits require entry <= optional cache <= shared hard cap")
endif()

function(axiom_render_memory_compile_definitions target)
    target_compile_definitions(${target} PRIVATE
        AXIOM_RENDER_CACHE_MIB=${AXIOM_RENDER_CACHE_MIB}
        AXIOM_RENDER_SHARED_MIB=${AXIOM_RENDER_SHARED_MIB}
        AXIOM_RENDER_ENTRY_MIB=${AXIOM_RENDER_ENTRY_MIB})
endfunction()
