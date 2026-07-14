cmake_minimum_required(VERSION 3.30)
project(Sandbox CXX)
set(CMAKE_CXX_STANDARD 20)

# The Seraph engine you built. CMakePresets.json passes -DSeraph_DIR; or set it
# yourself: -DSeraph_DIR=<engine-build-dir> (where SeraphConfig.cmake lives).
find_package(Seraph REQUIRED)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

# libGame: this project's native C++ gameplay scripts. Loaded at runtime by the
# editor/runtime via SDL_LoadObject (NOT linked into them); its SP_REGISTER_SCRIPT
# initializers register into libSeraph's ScriptRegistry on load.
add_library(Game SHARED ${GAME_SOURCES})
target_link_libraries(Game PRIVATE Seraph::Seraph)

# rpath: the engine bin (dev-time load) + @loader_path/.. (so a PACKAGED libGame,
# which sits in <game>/cache/, finds libSeraph one dir up next to the exe).
if (APPLE)
    set(GAME_RPATH "@loader_path/..;@loader_path;${SERAPH_ENGINE_BUILD_DIR}/bin")
elseif (UNIX)
    set(GAME_RPATH "$ORIGIN/..;$ORIGIN;${SERAPH_ENGINE_BUILD_DIR}/bin")
endif ()
set_target_properties(Game PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cache"
    BUILD_RPATH   "${GAME_RPATH}"
    INSTALL_RPATH "${GAME_RPATH}"
    DEBUG_POSTFIX "")

# IDE convenience targets:
#   RunEditor   — open this project in the Seraph editor
#   PackageGame — build a runnable, distributable game folder
add_custom_target(RunEditor
    COMMAND "${SERAPH_EDITOR_EXECUTABLE}"
            --project "${CMAKE_CURRENT_SOURCE_DIR}/Sandbox.sproj"
    USES_TERMINAL)
add_custom_target(PackageGame
    COMMAND "${SERAPH_EDITOR_EXECUTABLE}"
            --package "${CMAKE_CURRENT_SOURCE_DIR}/Sandbox.sproj"
    USES_TERMINAL)
