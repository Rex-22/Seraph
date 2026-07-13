cmake_minimum_required(VERSION 3.30)
project(Seraph)

set(CMAKE_CXX_STANDARD 20)

include(cmake/vendor.cmake)
include(cmake/config.cmake)

add_compile_definitions("SP_DEBUG=$<CONFIG:Debug>")

# The bundled sample project. Its asset dir is the FileSystem's fallback root,
# and its .sproj is what the editor opens by default during development. Both are
# compile-time conveniences only — a runtime-opened project overrides them.
set(SERAPH_DEFAULT_PROJECT "${PROJECT_SOURCE_DIR}/projects/Sandbox")

# Set option to control setting the resource path variable
option(USE_INSTALL_ASSET_PATH "Set resource path to install location" OFF)

if(NOT USE_INSTALL_ASSET_PATH)
  set(ASSET_PATH "${SERAPH_DEFAULT_PROJECT}/assets/")
else()
  set(ASSET_PATH "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/assets/")
endif()

install(DIRECTORY projects/Sandbox
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/projects"
)

# Dev-only paths for the editor's shader-cook step: the shaderc tool built as
# part of bgfx, and the include dirs its shaders need (bgfx headers + the
# engine's shared shader lib). These are build-tree paths; a shipped editor
# would ship the tool alongside instead.
set(SERAPH_SHADERC_PATH "${CMAKE_BINARY_DIR}/vendor/bgfx/cmake/bgfx/shaderc")
set(SERAPH_SHADER_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/vendor/bgfx/bgfx/src;${PROJECT_SOURCE_DIR}/vendor/bgfx/bgfx/examples/common;${PROJECT_SOURCE_DIR}/shader")

# Generate config.h into the build's generated-include dir. That dir is on the
# include search path (${CMAKE_BINARY_DIR}/include, see Seraph/CMakeLists.txt)
# for the engine and its consumers, so `#include <config.h>` resolves.
configure_file(Seraph/src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

# projects
add_subdirectory(Seraph)         # Seraph engine (static library)
add_subdirectory(Seraph-Editor)  # editor tool (executable, links Seraph)
add_subdirectory(Seraph-Runtime) # shipped-game player (executable, links Seraph)
