//
// Seraph engine — single public include for client applications.
//
// Include this from your application's source files to access the engine API.
//
// NOTE: This header intentionally does NOT include "core/EntryPoint.h" (which
// defines main()). Include EntryPoint.h in exactly ONE translation unit of your
// application — the one that implements Core::CreateApplication().
//

#pragma once

// --- Core ------------------------------------------------------------------
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/Math.h"
#include "Seraph/Core/Transform.h"

// --- Graphics --------------------------------------------------------------
#include "Seraph/Graphics/Camera.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Graphics/Texture2D.h"
#include "Seraph/Graphics/TextureAtlas.h"
#include "Seraph/Graphics/Material/ColorProperty.h"
#include "Seraph/Graphics/Material/FloatProperty.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/MaterialProperty.h"
#include "Seraph/Graphics/Material/TextureProperty.h"
#include "Seraph/Graphics/Material/Vector3Property.h"
#include "Seraph/Graphics/Material/Vector4ArrayProperty.h"
#include "Seraph/Graphics/Material/Vector4Property.h"

// --- Events ----------------------------------------------------------------
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Events/MouseEvent.h"
#include "Seraph/Events/WindowEvent.h"
#include "Seraph/Events/Seraph.h"

// --- Scene -----------------------------------------------------------------
#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Components/IDComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/CameraComponent.h"

// --- Platform --------------------------------------------------------------
#include "Platform/Window.h"
