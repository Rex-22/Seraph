# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Seraph is a voxel-based game engine built with modern C++20, using SDL3 for windowing, bgfx for cross-platform rendering, and Dear ImGui for UI. The project evolved from sdl-bgfx-imgui-starter and focuses on chunk-based world rendering with a material system and texture atlasing.

## Build System

### Initial Setup
CMake 3.30+ is required. The build system uses a superbuild approach that automatically downloads and builds all third-party dependencies (SDL3, bgfx, Dear ImGui, spdlog, glm).

### Build Commands

**Configure:**
```bash
# macOS/Linux
./configure-ninja.sh      # Preferred (faster)
./configure-make.sh       # Alternative

# Windows
configure-ninja.bat       # Preferred
configure-vs-22.bat       # Visual Studio 2022
configure-vs-19.bat       # Visual Studio 2019
```

**Build:**
```bash
# macOS/Linux
cmake --build build/debug-ninja     # Debug build
cmake --build build/release-ninja   # Release build

# Windows (Ninja)
cmake --build build\debug-ninja
cmake --build build\release-ninja

# Windows (Visual Studio)
cmake --build build\vs<year> --config Debug
cmake --build build\vs<year> --config Release
```

**Shader Compilation:**
Shaders are automatically compiled during the CMake build process via the `Shaders` target. They are compiled as headers and placed in `build/include/generated/shaders/`. Manual shader compilation scripts are not present in the current configuration - shaders are built through CMake's `bgfx_compile_shaders` function defined in shader/CMakeLists.txt.

**Run:**
```bash
# macOS/Linux
./build/debug-ninja/Seraph
./build/release-ninja/Seraph

# Windows
build\debug-ninja\Seraph.exe
build\release-ninja\Seraph.exe
```

## Architecture

### Core Application Flow
The application entry point is `src/main.cpp`, which creates and runs a `Core::Application` instance. The `Application` class (src/core/Application.h) manages the entire application lifecycle:
- Window creation and event handling
- Renderer initialization
- Main game loop with ImGui integration
- Camera controls and input handling
- Chunk world rendering

### Directory Structure
- **src/core/** - Core application infrastructure (Application, Log)
- **src/platform/** - Platform abstraction (Window)
- **src/graphics/** - Rendering components (Camera, Mesh, Material system, ChunkMesh, TextureAtlas)
- **src/world/** - Voxel world logic (Block, BlockState, Chunk)
- **src/resources/** - Resource loading and management (BlockModelLoader, ModelBakery, TextureManager, BlockStateLoader)
- **src/bgfx-imgui/** - bgfx backend for Dear ImGui
- **shader/** - Shader files (.sc files) compiled by bgfx's shaderc
- **assets/** - JSON models, blockstates, textures, and resource pack metadata

### Key Systems

#### JSON Block Model System (Minecraft-Compatible)
**NEW**: Seraph uses a complete JSON-driven block model and texture system matching Minecraft's format.

**Pipeline**:
```
JSON Files → BlockStateLoader → BlockModelLoader → ModelBakery → BakedModel → Chunk → Rendering
```

**Core Components**:
- **BlockState** (src/world/BlockState.h) - Links Block to BakedModel, stores properties and stateId
- **BlockModelLoader** (src/resources/) - Parses JSON models with parent inheritance and texture variable resolution
- **ModelBakery** (src/resources/) - Compiles BlockModel to BakedModel with quads, UVs, normals, and AO weights
- **BlockStateLoader** (src/resources/) - Parses blockstate JSON and creates BlockState objects
- **TextureManager** (src/resources/) - Manages texture atlases and provides texture lookup by resource name

**Storage**:
- Chunks store `BlockStateId` (uint16_t) for efficient storage - Minecraft-compatible
- BlockState registry provides O(1) lookup: `Blocks::GetStateById(stateId)`
- Block registry provides name lookup: `Blocks::GetByName("dirt")`

**JSON Format**:
- **Blockstates**: `assets/blockstates/<name>.json` - Maps state properties to models
- **Models**: `assets/models/block/<name>.json` - Defines geometry with cuboid elements
- **Textures**: `assets/textures/block/<name>.png` - Individual texture files
- **Pack metadata**: `assets/pack.mcmeta` - Resource pack information

For detailed JSON format reference, see `.agent/JSON_SPEC.md`

#### Voxel World System
- **Chunk**: 32x32x32 block grid stored as `std::array<BlockStateId, ChunkVolume>` (flat array)
- **Block**: Base class with visual properties (TransparencyType, light emission, AO flag, name)
- **BlockState**: State with properties, links to BakedModel for rendering
- **ChunkMesh**: Generates mesh from BakedModel quads with face culling and AO support

#### Material System
The material system (src/graphics/material/) provides a flexible property-based approach:
- **Material**: Wraps bgfx::ProgramHandle with dynamic properties via `AddProperty<T>(name, ...)`
- **MaterialProperty**: Base class for shader uniform types
- Property types: TextureProperty, ColorProperty, FloatProperty, Vector3Property, Vector4Property, Vector4ArrayProperty
- Materials are applied to render state via `Material::Apply(viewId, state)`

#### Graphics Pipeline
- **Camera**: First-person camera with pitch/yaw/roll rotation using quaternions, FOV, and projection matrices
- **Renderer**: Handles bgfx initialization and ImGui setup
- **Mesh**: Manages bgfx vertex/index buffers with dynamic update support
- **TextureAtlas**: Combines multiple block textures into a single atlas for efficient rendering

#### Shader Workflow
Shaders use bgfx's .sc format with varying definitions:
- Vertex shaders: shader/vs_*.sc, shader/chunk/vs_chunk.sc
- Fragment shaders: shader/fs_*.sc, shader/chunk/fs_chunk.sc
- Varying definitions: shader/varying.def.sc, shader/chunk/varying.def.sc
- Common includes: shader/bgfx_shader.sh, shader/common.sh, shader/shaderlib.sh

Shaders are compiled to headers via CMake's `bgfx_compile_shaders()` function and included directly in C++ code.

## Development Conventions

### Namespaces
Code is organized into namespaces matching directory structure:
- `Core::` - Core systems
- `Platform::` - Platform layer
- `Graphics::` - Rendering
- `World::` - Voxel world

### Coordinate System
- World space uses right-handed coordinate system
- Chunk blocks indexed as: `index = Z * ChunkSize * ChunkSize + Y * ChunkSize + X`
- Block positions represented as `BlockPos{X, Y, Z}` with uint16_t components

### Resource Management
- Asset path configured via CMake variable `ASSET_PATH` (defaults to `${PROJECT_SOURCE_DIR}/assets/`)
- Config file generated from src/config.h.in
- bgfx handles are managed carefully - use BGFX_INVALID_HANDLE for initialization

### Logging
Use the CORE_INFO, CORE_WARN, CORE_ERROR macros for logging throughout the application (defined in src/core/Log.h).

## Adding New Blocks

To add a new block to the system:

### 1. Create Blockstate JSON
Create `assets/blockstates/<block_name>.json`:
```json
{
  "variants": {
    "": { "model": "block/<block_name>" }
  }
}
```

### 2. Create Model JSON
Create `assets/models/block/<block_name>.json`:
```json
{
  "parent": "block/cube_all",
  "textures": {
    "all": "block/<texture_name>"
  }
}
```

Or for multi-textured blocks:
```json
{
  "parent": "block/cube_bottom_top",
  "textures": {
    "bottom": "block/<bottom_texture>",
    "top": "block/<top_texture>",
    "side": "block/<side_texture>"
  }
}
```

### 3. Add Texture
Place texture file in `assets/textures/block/<texture_name>.png` (16x16 recommended)

### 4. Register in Code
In `src/world/Blocks.cpp` → `RegisterBlocks()`:
```cpp
// Create block
auto* myBlock = RegisterBlock<Block>()
    ->SetName("my_block")
    ->SetIsOpaque(true)
    ->SetTransparencyType(TransparencyType::Opaque);

// Load blockstate and bake model
auto states = stateLoader->LoadBlockState("my_block", myBlock);
if (!states.empty()) {
    MyBlockState = states[0];
    RegisterBlockState(MyBlockState);
}
```

### 5. Add Static Pointer (Optional)
In `src/world/Blocks.h`:
```cpp
static BlockState* MyBlockState;
```

The block is now available for use in chunks via `Blocks::MyBlockState->GetStateId()`

## JSON Model Documentation

For comprehensive JSON format documentation, see:
- **`.agent/JSON_SPEC.md`** - Complete Minecraft JSON format specification
- **`.agent/ARCHITECTURE.md`** - System architecture and design
- **`.agent/QUICK_REFERENCE.md`** - Quick lookup guide

Example models available in `assets/models/block/`:
- `cube.json` - Base cube with 6 different face textures
- `cube_all.json` - Cube with same texture on all sides
- `cube_bottom_top.json` - Cube with different top/bottom textures
- `dirt.json`, `stone.json`, `grass_block.json`, `glass.json` - Block examples