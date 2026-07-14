cmake_minimum_required(VERSION 3.30)
project(Seraph)

set(CMAKE_CXX_STANDARD 20)

# The engine is a SHARED library, and per-project script modules are loadable
# dylibs that link it — so every static vendor lib absorbed into libSeraph must
# be position-independent. Set before the vendor libs are created so they all
# inherit it (spdlog/imgui/imguizmo/Jolt/SDL-static aren't PIC by default).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Keep vendor libs STATIC so they're absorbed into libSeraph — we want ONLY
# libSeraph (and per-project libGame) to be shared, not a scatter of vendor
# dylibs. Our own libSeraph/libGame use an explicit SHARED type, which overrides
# this. FORCE because it may otherwise linger ON in the CMake cache.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Vendor libs static, absorbed into libSeraph" FORCE)

# Co-locate the engine dylib and the executables in one bin/ dir so the exes find
# libSeraph via a relative rpath (and, on Windows, an adjacent DLL). Per-project
# libGame overrides its own output dir to the project's cache/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Relative rpath so a co-located libSeraph resolves. macOS: @executable_path;
# Linux: $ORIGIN; Windows ignores rpath (adjacent DLL). Consumed by the exes.
if (APPLE)
    set(SP_EXECUTABLE_RPATH "@executable_path")
elseif (UNIX)
    set(SP_EXECUTABLE_RPATH "$ORIGIN")
endif ()

include(cmake/vendor.cmake)
include(cmake/config.cmake)

add_compile_definitions("SP_DEBUG=$<CONFIG:Debug>")

# The bundled sample project. Its asset dir is the FileSystem's fallback root,
# and its .sproj is what the editor opens by default during development. Both are
# compile-time conveniences only — a runtime-opened project overrides them.
set(SERAPH_DEFAULT_PROJECT "${PROJECT_SOURCE_DIR}/projects/Sandbox")

# Which project's native C++ scripts (its "Game" module) are compiled into the
# editor/runtime. Defaults to the bundled Sandbox; point it at another project
# dir and reconfigure to build that game's scripts instead. One game is linked
# at a time, so the module's target name (Game) never collides.
set(SERAPH_GAME_DIR "${SERAPH_DEFAULT_PROJECT}" CACHE PATH
    "Project whose C++ scripts are compiled into the editor/runtime")

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

# Engine locations + cmake, baked so the editor can rebuild a project's Game
# module in-place (the "Compile Scripts" action reconfigures this build tree with
# a new SERAPH_GAME_DIR and builds the Game target).
set(SERAPH_ENGINE_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(SERAPH_ENGINE_BUILD_DIR "${CMAKE_BINARY_DIR}")
set(SERAPH_CMAKE_COMMAND "${CMAKE_COMMAND}")

# Generate config.h into the build's generated-include dir. That dir is on the
# include search path (${CMAKE_BINARY_DIR}/include, see Seraph/CMakeLists.txt)
# for the engine and its consumers, so `#include <config.h>` resolves.
configure_file(Seraph/src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

# projects
add_subdirectory(Seraph)         # Seraph engine (SHARED library: libSeraph)
# The active project's native C++ scripts, built as a SHARED library (libGame)
# that links libSeraph and is loaded at runtime via SDL_LoadObject — NOT linked
# into the hosts. Explicit binary dir because SERAPH_GAME_DIR may be out-of-tree.
# A default build still produces the active project's libGame (in its cache/).
add_subdirectory(${SERAPH_GAME_DIR} ${CMAKE_BINARY_DIR}/Game)
add_subdirectory(Seraph-Editor)  # editor tool (executable, links libSeraph)
add_subdirectory(Seraph-Runtime) # shipped-game player (links libSeraph)
