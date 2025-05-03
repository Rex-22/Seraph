cmake_minimum_required(VERSION 3.30)
project(Seraph)

set(CMAKE_CXX_STANDARD 20)

include(cmake/vendor.cmake)
include(cmake/config.cmake)

set(PROJECT ${PROJECT_NAME})

add_compile_definitions("GT_DEBUG=$<CONFIG:Debug>")

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

# Generate config.h
configure_file(src/config.h.in src/config.h)

# projects
add_subdirectory(src)
add_dependencies(${PROJECT} Shaders)
