# Sandbox game module: this project's native C++ gameplay scripts.
#
# Built as a SHARED library (libGame) that the editor/runtime load at runtime via
# SDL_LoadObject — NOT linked into the hosts. On load, its SP_REGISTER_SCRIPT
# static initializers run and register into libSeraph's ScriptRegistry (one
# registry, because the engine is one shared lib). See docs/scripting-overview.md.
set(PROJECT Game)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(Game SHARED ${GAME_SOURCES})

# Scripts call engine APIs. PRIVATE — nothing links Game; it links libSeraph
# (which carries the engine + vendor include dirs).
target_link_libraries(Game PRIVATE Seraph)

# libGame lands in the project's cache/ (next to its asset pack, so it ships with
# the project), and gets an rpath to the engine bin/ so it resolves libSeraph
# when loaded. DEBUG_POSTFIX "" keeps the filename stable (libGame, never
# libGamed) so the runtime loader finds it regardless of build type.
set_target_properties(Game PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cache"
    BUILD_RPATH "${CMAKE_BINARY_DIR}/bin"
    INSTALL_RPATH "${CMAKE_BINARY_DIR}/bin"
    DEBUG_POSTFIX ""
    FOLDER Libraries)
