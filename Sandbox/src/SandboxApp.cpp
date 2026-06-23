//
// Sandbox client application entry point.
//

#include <Seraph.h>

// EntryPoint.h defines main() and must be included in exactly ONE translation
// unit — this one. It calls Core::CreateApplication(), implemented below.
#include <core/EntryPoint.h>

#include "ExampleLayer.h"

namespace Sandbox
{

class SandboxApp : public Core::Application
{
public:
    SandboxApp() { PushLayer(new ExampleLayer()); }
    ~SandboxApp() override = default;
};

} // namespace Sandbox

Core::Application* Core::CreateApplication()
{
    return new Sandbox::SandboxApp();
}