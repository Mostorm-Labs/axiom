# Install script for directory: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/sdk/web

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/qing/Desktop/sources/git/deps/axiom/darwin-arm64/emsdk/upstream/emscripten/cache/sysroot")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE DIRECTORY FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/api")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/axiom.mjs")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/axiom.d.mts")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/package.json")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/README.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/example.html")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/serve.py")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE DIRECTORY FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/licenses")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES
    "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/pointer_input.mjs"
    "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/text_input.mjs"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js/internal" TYPE FILE FILES "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/internal/axiom.mjs")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/js" TYPE FILE FILES
    "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/axiom.wasm"
    "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/axiom.data"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/ur0b/p36-r1/build-qualified-64-192/sdk/web/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
