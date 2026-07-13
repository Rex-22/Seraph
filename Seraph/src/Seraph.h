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
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/GlmCustomFormatters.h"
#include "Seraph/Core/Input.h"
#include "Seraph/Core/KeyCodes.h"
#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/LogCustomFormatters.h"

// --- Graphics --------------------------------------------------------------
#include "Seraph/Graphics/Camera.h"
#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/MeshFactory.h"
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

// --- Asset -----------------------------------------------------------------
#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPack.h"
#include "Seraph/Asset/Pack/AssetPackBuilder.h"
#include "Seraph/Asset/Pack/RuntimeAssetManager.h"

// --- Events ----------------------------------------------------------------
#include "Seraph/Events/Events.h"
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Events/MouseEvent.h"
#include "Seraph/Events/WindowEvent.h"

// --- Scene -----------------------------------------------------------------
#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/SceneAsset.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/IDComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/RelationshipComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"

// --- Project / Runtime ------------------------------------------------------
#include "Seraph/Project/Project.h"
#include "Seraph/Runtime/RuntimeLayer.h"

// --- Editor ----------------------------------------------------------------
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Editor/EditorLayer.h"
#include "Seraph/Editor/Panels/EntityBrowserPanel.h"
#include "Seraph/Editor/Panels/EntityInspectorPanel.h"
#include "Seraph/Editor/Panels/EditorGizmo.h"
#include "Seraph/Editor/Panels/ViewportPanel.h"

// --- Graphics (render target) ----------------------------------------------
#include "Seraph/Graphics/RenderTarget.h"

// --- Platform --------------------------------------------------------------
#include "Platform/Window.h"
