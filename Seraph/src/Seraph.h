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
#include "core/Base.h"
#include "core/Application.h"
#include "core/Layer.h"
#include "core/Log.h"
#include "core/Math.h"
#include "core/Transform.h"

// --- Graphics --------------------------------------------------------------
#include "graphics/Camera.h"
#include "graphics/Mesh.h"
#include "graphics/Renderer.h"
#include "graphics/material/Material.h"
#include "graphics/material/ColorProperty.h"
#include "graphics/material/TextureProperty.h"

// --- Asset / resource helpers ----------------------------------------------
#include "core/Core.h"

// --- Events ----------------------------------------------------------------
#include "events/Event.h"
#include "events/KeyEvent.h"
#include "events/MouseEvent.h"
#include "events/KeyEvent.h"