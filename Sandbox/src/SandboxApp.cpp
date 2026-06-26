//
// Sandbox client application entry point.
//

#include <Seraph.h>

// EntryPoint.h defines main() and must be included in exactly ONE translation
// unit — this one. It calls CreateApplication(), implemented below.
#include <Seraph/Core/EntryPoint.h>

#include "ExampleLayer.h"

namespace Sandbox
{

class SandboxApp : public Seraph::Application
{
public:
    SandboxApp() { PushLayer(new ExampleLayer()); }
    ~SandboxApp() override = default;
};

} // namespace Sandbox

Seraph::Application* Seraph::CreateApplication()
{
    return new Sandbox::SandboxApp();
}