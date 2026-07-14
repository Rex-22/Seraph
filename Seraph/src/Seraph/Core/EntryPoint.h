//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Application.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Physics/PhysicsSystem.h"

extern Seraph::Application* CreateApplication();

int main(int argc, char** argv)
{
    Seraph::Log::Init();
    // File access is available before the Application exists, so a client's
    // CreateApplication() can read a project file to shape its window/spec.
    Seraph::FileSystem::Init();
    // Process-global Jolt state; must outlive any scene that creates bodies.
    Seraph::PhysicsSystem::Init();

    auto app = Seraph::CreateApplication();
    app->Run();

    delete app;

    Seraph::PhysicsSystem::Shutdown();
    Seraph::FileSystem::Shutdown();
    Seraph::Log::Shutdown();
}
