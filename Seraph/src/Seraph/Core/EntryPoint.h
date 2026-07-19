//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Application.h"
#include "Seraph/Console/Console.h"
#include "Seraph/Core/CommandLine.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/Version.h"
#include "Seraph/Graphics/RenderSystem.h"
#include "Seraph/Physics/PhysicsSystem.h"
#include "Seraph/Settings/Settings.h"

extern Seraph::Application* CreateApplication();

int main(int argc, char** argv)
{
    // Parse the command line first so a client's CreateApplication() (and the
    // editor/runtime) can read flags like --project / --package.
    Seraph::CommandLine::Init(argc, argv);

    Seraph::Log::Init();
    SP_CORE_INFO_TAG("Core", "Seraph Engine v{}", Seraph::EngineVersion());
    // File access is available before the Application exists, so a client's
    // CreateApplication() can read a project file to shape its window/spec.
    Seraph::FileSystem::Init();
    // Settings registry: install the backend, register engine settings (bound to
    // stable static setting structs), then load Engine + User scopes so persisted
    // values apply to those fields before any subsystem consumes them.
    Seraph::Settings::Init();
    Seraph::PhysicsSystem::RegisterSettings();
    Seraph::RenderSystem::RegisterSettings();
    Seraph::Settings::LoadEngineUser();
    // Flush pending AutoCVar registrations + enable the dev console. After
    // LoadEngineUser so archived CVar values are already applied to their fields.
    Seraph::Console::Init();
    // Process-global Jolt state; must outlive any scene that creates bodies.
    Seraph::PhysicsSystem::Init();

    auto app = Seraph::CreateApplication();
    app->Run();

    delete app;

    Seraph::Console::Shutdown();
    // Save dirty scopes before the filesystem goes away.
    Seraph::Settings::Shutdown();
    Seraph::PhysicsSystem::Shutdown();
    Seraph::FileSystem::Shutdown();
    Seraph::Log::Shutdown();
}
