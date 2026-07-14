//
// Text templates for the C++/CMake files scaffolded into a newly-created
// project, so a fresh project ships with a buildable Game module and a starter
// script. Kept out of ProjectManager.cpp to keep that file readable.
//

#pragma once

#include <string>

namespace Seraph::ProjectTemplates
{

// CMakeLists.txt for a project's Game module — the same OBJECT-library shape the
// bundled Sandbox uses, so self-registering scripts are never dead-stripped.
inline std::string GameCMakeLists()
{
    return R"cmake(# Game module: this project's native C++ gameplay scripts.
#
# Built as an OBJECT library (not STATIC) so self-registering scripts
# (SP_REGISTER_SCRIPT) link directly into the editor/runtime and are never
# dead-stripped the way an unreferenced static-archive member would be.
set(PROJECT Game)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(Game OBJECT ${GAME_SOURCES})
target_link_libraries(Game PUBLIC Seraph)
set_target_properties(Game PROPERTIES FOLDER Libraries)
)cmake";
}

// A starter ScriptableEntity, ready to bind from the inspector.
inline std::string ExampleScriptHeader()
{
    return R"cpp(#pragma once

#include <Seraph/Scripts/ScriptableEntity.h>

// A starter script. Rename it, add gameplay in OnUpdate, then bind it to an
// entity from the inspector: Add Component > Script > ExampleScript.
class ExampleScript : public Seraph::ScriptableEntity
{
public:
    void OnCreate() override;
    void OnUpdate(f64 dt) override;
};
)cpp";
}

inline std::string ExampleScriptSource()
{
    return R"cpp(#include "ExampleScript.h"

#include <Seraph/Core/Log.h>
#include <Seraph/Scripts/ScriptRegistry.h>

void ExampleScript::OnCreate()
{
    SP_INFO_TAG("Scripting", "ExampleScript::OnCreate");
}

void ExampleScript::OnUpdate(f64 dt)
{
    (void)dt;
    // Your gameplay here. Read input with Seraph::Input::IsKeyDown(...),
    // move via Transform(), reach components with GetComponent<T>().
}

// Registers this class under the name a scene's ScriptComponent references.
SP_REGISTER_SCRIPT(ExampleScript, "ExampleScript")
)cpp";
}

inline std::string Readme(const std::string& projectName)
{
    return "# " + projectName + R"md(

A Seraph project. Its native C++ gameplay scripts live in `src/` and compile
into a `Game` module (see `CMakeLists.txt`).

## Building this project's scripts

Native scripts are compiled into the editor/runtime, so after adding or changing
a script you must rebuild. Point the engine build at this project and rebuild:

    cmake -S <seraph-repo> -B <build> -DSERAPH_GAME_DIR="<this-directory>"
    cmake --build <build> --target Seraph-Editor Seraph-Runtime

Then bind a script to an entity from the inspector: **Add Component > Script**.
)md";
}

} // namespace Seraph::ProjectTemplates
