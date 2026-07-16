//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Application.h"
#include "Seraph/Core/CommandLine.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/Version.h"
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
    // Settings registry: install the backend, then load Engine + User scopes.
    // (Engine settings register during subsystem init below; their persisted
    // values apply because they bind to stable static setting structs.)
    Seraph::Settings::Init();
    Seraph::Settings::LoadEngineUser();
    // Process-global Jolt state; must outlive any scene that creates bodies.
    Seraph::PhysicsSystem::Init();

    auto app = Seraph::CreateApplication();
    app->Run();

    delete app;

    // Save dirty scopes before the filesystem goes away.
    Seraph::Settings::Shutdown();
    Seraph::PhysicsSystem::Shutdown();
    Seraph::FileSystem::Shutdown();
    Seraph::Log::Shutdown();
}
