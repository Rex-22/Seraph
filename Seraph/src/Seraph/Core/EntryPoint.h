//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Application.h"

extern Seraph::Application* CreateApplication();

int main(int argc, char** argv)
{
    Seraph::Log::Init();

    auto app = Seraph::CreateApplication();
    app->Run();

    delete app;

    Seraph::Log::Shutdown();
}
