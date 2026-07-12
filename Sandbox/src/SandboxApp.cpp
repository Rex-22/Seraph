//
// Sandbox client application entry point.
//
// Minimal integration pattern:
//   1. Create your scene and call OnLoaded().
//   2. Create a SceneRenderer with your desired render settings.
//   3. Push EditorLayer (or your own layer) and run.
//

#include <Seraph.h>
#include <Seraph/Core/EntryPoint.h>

#include "ExampleScene.h"

namespace Sandbox
{

class SandboxApp : public Seraph::Application
{
public:
    SandboxApp()
        : Seraph::Application(Seraph::ApplicationSpecification{
              .Name = "Sandbox",
              .Window = { 1280, 720, "Seraph Sandbox", false },
              // Editor mode: loose files under ASSET_PATH. To run against a
              // built pack instead, set:
              //   .Assets = Seraph::ApplicationSpecification::AssetMode::Runtime,
              //   .AssetPack = "assets.pack",
          })
    {
        auto scene = Seraph::Ref<ExampleScene>::Create();
        scene->OnLoaded();

        Seraph::SceneRendererSettings settings{ glm::vec3(0.6f, 0.5f, 0.4f) };
        auto renderer = Seraph::Ref<Seraph::SceneRenderer>::Create(scene, settings);

        auto editor = Seraph::Ref<Seraph::EditorLayer>::Create(scene, renderer);
        editor->SetDefaultMaterial(scene->GetMaterial());
        PushLayer(editor);
    }

    ~SandboxApp() override = default;
};

} // namespace Sandbox

Seraph::Application* Seraph::CreateApplication()
{
    return new Sandbox::SandboxApp();
}
