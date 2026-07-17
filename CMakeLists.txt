cmake_minimum_required(VERSION 3.30)
# VERSION is the engine version, surfaced at runtime (config.h SERAPH_VERSION) and
# in the generated SeraphConfig package. No LANGUAGES restriction — vendored SDL
# and bgfx include C sources, so both C and CXX must stay enabled.
project(Seraph VERSION 0.1.0)

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
add_compile_definitions("SP_SHT_PARSE")

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

# Engine build dir + cmake, baked so the editor's "Compile Scripts" action can
# build a project's Game module standalone: it configures the project's own build
# tree with -DSeraph_DIR=<this build> (where SeraphConfig.cmake lives) and builds
# the Game target. SERAPH_ENGINE_SOURCE_DIR is also handy for tooling/docs.
set(SERAPH_ENGINE_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(SERAPH_ENGINE_BUILD_DIR "${CMAKE_BINARY_DIR}")
set(SERAPH_CMAKE_COMMAND "${CMAKE_COMMAND}")

# Generate config.h into the build's generated-include dir. That dir is on the
# include search path (${CMAKE_BINARY_DIR}/include, see Seraph/CMakeLists.txt)
# for the engine and its consumers, so `#include <config.h>` resolves.
configure_file(Seraph/src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

# --- SDK package: a hand-written SeraphConfig.cmake so a game project can
# find_package(Seraph) against THIS local engine build (source-SDK model). It
# defines an imported Seraph::Seraph target pointing at the built libSeraph plus
# every include dir + compile def a Game module needs, and exposes the editor/
# runtime exe paths. Paths are this machine's build tree — valid because every
# machine builds the engine locally. A project points -DSeraph_DIR at this build.
include(CMakePackageConfigHelpers)
set(SERAPH_SDK_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/Seraph/src"
    "${CMAKE_BINARY_DIR}/include"
    ${SDL3_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR} ${BGFX_INCLUDE_DIR} ${BX_INCLUDE_DIR}
    ${BIMG_INCLUDE_DIR} ${SPDLOG_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${YAML_INCLUDE_DIR}
    ${ENTT_INCLUDE_DIR} ${IMGUIZMO_INCLUDE_DIR} ${ASSIMP_INCLUDE_DIR}
    ${JOLT_INCLUDE_DIR})
# Compile defs a Game module must build with to match libSeraph's ABI: GLM
# config plus the ABI-affecting Jolt defines (SERAPH_JOLT_ABI_DEFINITIONS, from
# cmake/jolt.cmake). Baked into SeraphConfig's INTERFACE_COMPILE_DEFINITIONS so
# JPH_DEBUG_RENDERER / JPH_OBJECT_LAYER_BITS can't drift between the engine and a
# consumer that includes a Jolt-backed header.
set(SERAPH_SDK_COMPILE_DEFINITIONS
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    ${SERAPH_JOLT_ABI_DEFINITIONS})
set(SERAPH_DYLIB_PATH
    "${CMAKE_BINARY_DIR}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}Seraph${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(SERAPH_IMPLIB_PATH "${CMAKE_BINARY_DIR}/bin/Seraph${CMAKE_IMPORT_LIBRARY_SUFFIX}")
set(SERAPH_EDITOR_EXE  "${CMAKE_BINARY_DIR}/bin/Seraph-Editor${CMAKE_EXECUTABLE_SUFFIX}")
set(SERAPH_RUNTIME_EXE "${CMAKE_BINARY_DIR}/bin/Seraph-Runtime${CMAKE_EXECUTABLE_SUFFIX}")
configure_file(cmake/SeraphConfig.cmake.in
    "${CMAKE_BINARY_DIR}/SeraphConfig.cmake" @ONLY)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/SeraphConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)

# projects. Game modules are NOT built here — every project (incl. Sandbox) is a
# standalone find_package(Seraph) build (its own dir/build), built by the editor's
# Compile Scripts action or its own IDE. The engine build just produces the
# engine + editor + runtime + the SeraphConfig package above.
# SeraphHeaderTool: the reflection code-gen tool (libclang-based). Opt-in while
# in development — it needs libclang installed. When ON, libSeraph generates
# reflection registrations from SPROPERTY/SCLASS annotations via sht_reflect()
# (see cmake/sht.cmake). When OFF (default), the engine builds with no libclang
# dependency and simply omits the generated registrations. Added BEFORE the
# engine so the SeraphHeaderTool target exists when Seraph calls sht_reflect().
option(SERAPH_BUILD_HEADER_TOOL "Build SeraphHeaderTool (reflection code-gen; needs libclang)" ON)
if(SERAPH_BUILD_HEADER_TOOL)
    add_subdirectory(Tools/SeraphHeaderTool)
endif()

add_subdirectory(Seraph)         # Seraph engine (SHARED library: libSeraph)
add_subdirectory(Seraph-Editor)  # editor tool (executable, links libSeraph)
add_subdirectory(Seraph-Runtime) # shipped-game player (links libSeraph)
