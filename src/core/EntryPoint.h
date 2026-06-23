//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Base.h"
#include "Application.h"

extern Core::Application* Core::CreateApplication();

int main(int argc, char** argv)
{
    Core::Log::Init();

    auto app = Core::CreateApplication();
    app->Run();

    delete app;
}
