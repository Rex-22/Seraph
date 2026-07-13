cmake_minimum_required(VERSION 3.30)
project(Seraph)

set(CMAKE_CXX_STANDARD 20)

include(cmake/vendor.cmake)
include(cmake/config.cmake)

add_compile_definitions("SP_DEBUG=$<CONFIG:Debug>")

# Set option to control setting the resource path variable
option(USE_INSTALL_ASSET_PATH "Set resource path to install location" OFF)

if(NOT USE_INSTALL_ASSET_PATH)
  set(ASSET_PATH "${PROJECT_SOURCE_DIR}/assets/")
else()
  set(ASSET_PATH "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/assets/")
endif()

install(DIRECTORY assets
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}"
)

# Generate config.h into the build's generated-include dir. That dir is on the
# include search path (${CMAKE_BINARY_DIR}/include, see Seraph/CMakeLists.txt)
# for the engine and its consumers, so `#include <config.h>` resolves.
configure_file(Seraph/src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

# projects
add_subdirectory(Seraph)         # Seraph engine (static library)
add_subdirectory(Seraph-Editor)  # editor tool (executable, links Seraph)
add_subdirectory(Seraph-Runtime) # shipped-game player (executable, links Seraph)
