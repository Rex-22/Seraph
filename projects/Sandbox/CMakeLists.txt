# Sandbox game module: this project's native C++ gameplay scripts.
#
# Built as an OBJECT library (not STATIC) on purpose: its objects are linked
# directly into the editor/runtime executables, so a self-registering script
# (SP_REGISTER_SCRIPT) is never dead-stripped the way an unreferenced member of
# a static archive would be. See docs/scripting-overview.md.
set(PROJECT Game)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(Game OBJECT ${GAME_SOURCES})

# Scripts call engine APIs (Entity, components, ScriptableEntity, ScriptRegistry).
# PUBLIC so anything linking Game also sees Seraph (and its vendor includes).
target_link_libraries(Game PUBLIC Seraph)

set_target_properties(Game PROPERTIES FOLDER Libraries)
