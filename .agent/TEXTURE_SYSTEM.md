# Texture System Architecture & Analysis

## Overview

Seraph's texture system uses a Minecraft-compatible approach with texture atlasing, JSON-driven model definitions, and UV-based texture mapping. This document provides a comprehensive reference for how textures flow from PNG files to rendered voxels.

---

## System Architecture

### Pipeline Flow

```
PNG Files → TextureManager::ScanTextures() → TextureAtlasBuilder → TextureAtlas
                                                                         ↓
                                                        TextureManager::m_TextureRegistry
                                                                         ↓
JSON Models → BlockModelLoader → ModelBakery::ResolveTextureUVs() → BakedQuad
                                                                         ↓
                                                            ChunkMesh → Vertices → Shader
```

---

## Phase 1: Texture Loading & Atlas Building

### Step 1: Texture Scanning
**File**: `src/resources/TextureManager.cpp:102-136`

**Function**: `TextureManager::ScanTextures(const std::string& directory)`

**Process**:
1. Recursively scans `assets/textures/block/` directory
2. For each `.png` file:
   - Strips file extension
   - Adds `"block/"` prefix to create resource name
   - Example: `grass_block_side.png` → `"block/grass_block_side"`
3. Calls `TextureAtlasBuilder::AddTexture(resourceName, filePath)`

**Example**:
```cpp
// Scanning assets/textures/block/
// Finds: grass_block_side.png
// Creates: resourceName = "block/grass_block_side"
// Full path: assets/textures/block/grass_block_side.png
```

### Step 2: Texture Loading
**File**: `src/resources/TextureAtlasBuilder.cpp:190-205`

**Function**: `TextureAtlasBuilder::LoadPNG(const std::string& filePath, TextureData& outData)`

**Process**:
1. Uses `stbi_load()` to read PNG file
2. Loads as RGBA (4 channels)
3. Stores raw pixel data in `TextureData` struct:
   ```cpp
   struct TextureData {
       std::string resourceName;  // "block/grass_block_side"
       std::string filePath;       // Full path to PNG
       int width, height;          // Texture dimensions (usually 16x16)
       unsigned char* pixels;      // Raw RGBA data
   };
   ```

**Error Handling**:
- Returns `false` if file not found or stbi fails
- Logs error: `"Failed to load PNG: {filePath}"`

### Step 3: Atlas Packing
**File**: `src/resources/TextureAtlasBuilder.cpp:207-303`

**Function**: `TextureAtlasBuilder::PackTextures(int spriteSize, int padding)`

**Process**:
1. **Calculate Grid Dimensions** (Lines 207-241):
   - Counts total textures
   - Calculates square grid size: `gridWidth = ceil(sqrt(textureCount))`
   - Rounds up to power of 2: `atlasWidth = nextPowerOf2(gridWidth * spriteSize)`
   - Creates `atlasWidth × atlasHeight` pixel buffer

2. **Pack Textures into Grid** (Lines 243-303):
   - Iterates through all loaded textures
   - Places each texture at grid position `(x, y)`
   - Copies pixel data into atlas buffer
   - **Calculates UV coordinates**:
     ```cpp
     position.uvSize = glm::vec2(
         spriteSizeFloat / atlasWidthFloat,
         spriteSizeFloat / atlasHeightFloat
     );

     position.uvOffset.x = (x * spriteSizeFloat) / atlasWidthFloat;
     position.uvOffset.y = ((gridHeight - 1.0f - y) * spriteSizeFloat) / atlasHeightFloat;  // Y-flipped for OpenGL
     ```
   - Stores `AtlasPosition` in `m_Positions` map with resource name as key

3. **Debug Output** (Lines 118-157):
   - Saves `debug_atlas.png` with visual representation
   - Saves `debug_atlas_info.txt` with texture list and UV coordinates

**Example**:
```
Grid: 4×4 textures, 16×16 sprites → 64×64 pixel atlas
Texture "block/dirt" at grid (0,0):
  UV offset: (0.0, 0.75)    // Top-left in OpenGL coords
  UV size: (0.25, 0.25)     // 1/4 of atlas width/height
```

### Step 4: Atlas Registration
**File**: `src/resources/TextureManager.cpp:138-187`

**Function**: `TextureManager::BuildAtlases()`

**Process**:
1. Calls `TextureAtlasBuilder::BuildAtlas()` to get packed atlas
2. Creates `TextureAtlas` object with GPU texture
3. **Registers all textures** in `m_TextureRegistry` map (Lines 172-183):
   ```cpp
   for (const auto& [resourceName, position] : positions) {
       TextureInfo info;
       info.gridCoords = position.gridCoords;   // (x, y) in grid
       info.uvOffset = position.uvOffset;       // UV start [0-1]
       info.uvSize = position.uvSize;           // UV size [0-1]
       info.atlas = blocksAtlas;
       info.isAnimated = false;                 // TODO: .mcmeta support

       m_TextureRegistry[resourceName] = info;
   }
   ```

**Key Data Structure**:
```cpp
std::unordered_map<std::string, TextureInfo> m_TextureRegistry;
// Key: "block/grass_block_side"
// Value: TextureInfo with UV coordinates
```

---

## Phase 2: Model Loading & Texture Resolution

### Step 5: JSON Model Parsing
**File**: `src/resources/BlockModelLoader.cpp:252-302`

**Function**: `BlockModelLoader::ResolveTextures(BlockModel& model)`

**Process**:
1. Loads JSON model file (e.g., `assets/models/block/grass_block.json`)
2. **Resolves texture variables**:
   - Model defines: `"textures": { "side": "block/grass_block_side", "overlay": "block/grass_block_side_overlay" }`
   - Elements reference: `"texture": "#side"` or `"texture": "#overlay"`
   - Loader replaces `#side` → `"block/grass_block_side"`

**Example**:
```json
{
  "textures": {
    "side": "block/grass_block_side",
    "overlay": "block/grass_block_side_overlay"
  },
  "elements": [
    {
      "faces": {
        "north": { "texture": "#side" }      // Resolves to "block/grass_block_side"
      }
    },
    {
      "faces": {
        "north": { "texture": "#overlay" }   // Resolves to "block/grass_block_side_overlay"
      }
    }
  ]
}
```

**Logging**:
- Line 254: `"=== Resolving textures for model '{}' ==="`
- Line 271: `"Resolved texture variable '{}' = '{}'"`

### Step 6: Model Baking
**File**: `src/resources/ModelBakery.cpp:78-123`

**Function**: `ModelBakery::BakeFace(const BlockElement& element, const BlockFace& face, FaceDirection direction)`

**Process**:
1. Takes BlockElement and BlockFace from JSON model
2. Creates `BakedQuad` with:
   - **Vertex positions** (4 corners of face)
   - **UVs** (resolved from texture name)
   - **Normal** (calculated from face direction)
   - **Cullface** (for face culling optimization)
   - **AO weights** (for ambient occlusion)
   - **Shade flag** (whether to apply lighting)
   - **Tintindex** (for biome/block-specific tinting)

**Key Call** (Line 110):
```cpp
ResolveTextureUVs(face.texture, face.uv, textureAtlas, quad.uvs);
```

### Step 7: UV Resolution (Critical!)
**File**: `src/resources/ModelBakery.cpp:223-279`

**Function**: `ModelBakery::ResolveTextureUVs(const std::string& texturePath, const glm::vec4& elementUV, const Graphics::TextureAtlas* textureAtlas, glm::vec2 outUVs[4])`

**Process**:
1. **Lookup texture in registry**:
   ```cpp
   if (m_TextureManager && m_TextureManager->HasTexture(texturePath)) {
       TextureInfo texInfo = m_TextureManager->GetTextureInfo(texturePath);
       baseU = texInfo.uvOffset.x;
       baseV = texInfo.uvOffset.y;
       uvSpriteWidth = texInfo.uvSize.x;
       uvSpriteHeight = texInfo.uvSize.y;
   }
   ```

2. **If texture NOT FOUND** (Line 269):
   ```cpp
   CORE_ERROR("Texture '{}' NOT FOUND in atlas! Using fallback (0,0) - will render BLACK", texturePath);
   baseU = 0.0f;
   baseV = 0.0f;
   uvSpriteWidth = 1.0f;
   uvSpriteHeight = 1.0f;
   ```
   - **This is the bug**: Missing textures default to (0,0) which renders black!

3. **Calculate final UVs**:
   - Applies element UV coordinates (for partial texture usage)
   - Maps element UV [0-16] space to atlas UV [0-1] space
   - Returns 4 UV coordinates for quad corners

**Example**:
```
Texture: "block/grass_block_side"
Registry lookup: uvOffset=(0.25, 0.5), uvSize=(0.25, 0.25)
Element UV: [0, 0, 16, 16] (full texture)
Output UVs:
  [0] = (0.25, 0.75)   // Top-left
  [1] = (0.50, 0.75)   // Top-right
  [2] = (0.50, 0.50)   // Bottom-right
  [3] = (0.25, 0.50)   // Bottom-left
```

---

## Phase 3: Mesh Generation & Rendering

### Step 8: Chunk Mesh Building
**File**: `src/graphics/ChunkMesh.cpp:42-120`

**Function**: `ChunkMesh::GenerateMeshData(const World::Chunk* chunk)`

**Process**:
1. **Iterate through all blocks** in chunk (Lines 65-86):
   ```cpp
   for (uint16_t z = 0; z < ChunkSize; z++) {
       for (uint16_t y = 0; y < ChunkSize; y++) {
           for (uint16_t x = 0; x < ChunkSize; x++) {
               BlockStateId stateId = chunk->GetBlock(x, y, z);
               if (stateId == 0) continue;  // Air block

               World::BlockState* blockState = World::Blocks::GetStateById(stateId);
               BakedModel* bakedModel = blockState->GetBakedModel();
   ```

2. **Render all quads** from baked model (Lines 88-120):
   ```cpp
   for (const auto& quad : bakedModel->GetQuads()) {
       // Face culling check (skip if adjacent block is opaque)
       if (!quad.cullface.empty() && ShouldCullFace(...)) {
           continue;
       }

       // Add quad to mesh (opaque or transparent)
       if (block->GetTransparencyType() == World::TransparencyType::Opaque) {
           AddBakedQuad(quad, blockWorldPos, m_OpaqueVertices, m_OpaqueIndices);
       } else {
           AddBakedQuad(quad, blockWorldPos, m_TransparentVertices, m_TransparentIndices);
       }
   }
   ```

### Step 9: Vertex Generation
**File**: `src/graphics/ChunkMesh.cpp:207-262`

**Function**: `ChunkMesh::AddBakedQuad(const BakedQuad& quad, const glm::vec3& blockWorldPos, std::vector<ChunkVertex>& vertices, std::vector<uint16_t>& indices)`

**Process**:
1. **Creates 4 vertices** for the quad:
   ```cpp
   for (int i = 0; i < 4; i++) {
       ChunkVertex vertex;
       vertex.Position = blockWorldPos + quad.vertices[i];
       vertex.UV = quad.uvs[i];           // Texture coordinates from atlas
       vertex.AO = quad.aoWeights[i];     // Ambient occlusion
       vertices.push_back(vertex);
   }
   ```

2. **Creates 2 triangles** (6 indices) for the quad

**Current Vertex Format**:
```cpp
struct ChunkVertex {
    glm::vec3 Position;  // World position
    glm::vec2 UV;        // Texture coordinates [0-1]
    float AO;            // Ambient occlusion weight [0-1]
};
```

### Step 10: Shader Rendering
**Files**:
- `shader/chunk/varying.def.sc`
- `shader/chunk/vs_chunk.sc`
- `shader/chunk/fs_chunk.sc`

**Current Shader Flow**:
1. **Vertex Shader**:
   - Receives vertex position, UV, AO
   - Transforms position to clip space
   - Passes UV and AO to fragment shader

2. **Fragment Shader**:
   - Samples single texture: `vec4 color = texture2D(s_tex, v_uv);`
   - Applies AO: `color.rgb *= v_ao;`
   - Outputs final color

**Current Limitation**: Only supports **one texture per quad**. No overlay/blending support.

---

## Grass Block Bug: Root Cause Analysis

### The Problem

Grass blocks render with **black side textures** instead of the expected dirt+grass appearance.

### Grass Block JSON Structure

**File**: `assets/models/block/grass_block.json`

```json
{
  "parent": "block/block",
  "textures": {
    "particle": "block/dirt",
    "bottom": "block/dirt",
    "top": "block/grass_block_top",
    "side": "block/grass_block_side",
    "overlay": "block/grass_block_side_overlay"
  },
  "elements": [
    // ELEMENT 1: Base cube (dirt bottom, grass top, dirt-like sides)
    {
      "from": [0, 0, 0],
      "to": [16, 16, 16],
      "faces": {
        "down":  { "texture": "#bottom" },
        "up":    { "texture": "#top" },
        "north": { "texture": "#side" },
        "south": { "texture": "#side" },
        "west":  { "texture": "#side" },
        "east":  { "texture": "#side" }
      }
    },
    // ELEMENT 2: Overlay cube (only side faces, tintable for biomes)
    {
      "from": [0, 0, 0],
      "to": [16, 16, 16],
      "faces": {
        "north": { "texture": "#overlay", "tintindex": 0 },
        "south": { "texture": "#overlay", "tintindex": 0 },
        "west":  { "texture": "#overlay", "tintindex": 0 },
        "east":  { "texture": "#overlay", "tintindex": 0 }
      }
    }
  ]
}
```

### Expected Textures

- ✅ `assets/textures/block/grass_block_side.png` (359 bytes) - Brown dirt with green pixels
- ✅ `assets/textures/block/grass_block_side_overlay.png` (177 bytes) - Semi-transparent grass layer
- ✅ `assets/textures/block/grass_block_top.png` (338 bytes)
- ✅ `assets/textures/block/dirt.png` (230 bytes)

**All texture files exist** in the filesystem.

### Root Cause: Missing Overlay Texture Support

**Issue 1: Texture Lookup Failure (Likely)**
- `grass_block_side_overlay.png` may not be loaded into atlas
- Or texture name resolution fails during baking
- Result: `ModelBakery::ResolveTextureUVs()` returns (0,0) → **black rendering**

**Issue 2: No Multi-Layer Rendering (Confirmed)**
- `BakedQuad` only stores **one UV array**: `glm::vec2 uvs[4]`
- No support for overlay textures or blending
- Even if overlay texture loads, it renders as separate opaque quad (not blended)

**Issue 3: No Tint Support (Confirmed)**
- `BakedQuad` has `int tintindex` field but it's **not used** in rendering
- Overlay should be tinted based on biome (grass color)
- Current shader doesn't support tint colors

### How Minecraft Does It

1. **Base Layer**: Render `grass_block_side.png` (dirt texture with built-in green)
2. **Overlay Layer**: Render `grass_block_side_overlay.png` on top with alpha blending
3. **Biome Tinting**: Multiply overlay color by biome grass color (via `tintindex: 0`)

### Why Current System Fails

```
Element 1 (base):     texture="#side"    → "block/grass_block_side"    → ✅ Should work
Element 2 (overlay):  texture="#overlay" → "block/grass_block_side_overlay" → ❌ Fails

Possible failure points:
1. TextureManager::ScanTextures() misses the file (unlikely - file exists)
2. TextureAtlasBuilder::AddTexture() fails to load PNG (check debug_atlas.png)
3. Texture name mismatch in registry (e.g., "grass_block_side_overlay" vs "block/grass_block_side_overlay")
4. ModelBakery::ResolveTextureUVs() receives wrong texture path
5. Even if found, overlay renders as opaque black quad (no blending)
```

---

## Current System Limitations

### What Works
✅ Multiple elements per model (grass_block has 2 elements)
✅ Each element generates separate `BakedQuad` objects
✅ All quads are rendered in sequence
✅ Transparency sorting (opaque vs transparent meshes)
✅ Tint index stored in `BakedQuad` (but not used)

### What's Missing
❌ **Multi-texture quads** (each quad only has 1 texture)
❌ **Texture blending/layering** in shader
❌ **Overlay texture channel** in vertex data
❌ **Tint color application** (stored but not rendered)
❌ **Proper alpha blending** for overlays

---

## Solution: Implement Full Overlay Support

### Required Changes

#### 1. Extend BakedQuad Structure
**File**: `src/resources/BakedModel.h`

```cpp
struct BakedQuad {
    glm::vec3 vertices[4];
    glm::vec2 uvs[4];              // Base texture UVs
    glm::vec2 overlayUVs[4];       // NEW: Overlay texture UVs
    glm::vec3 normal;
    std::string cullface;
    float aoWeights[4];
    bool shade;
    int tintindex;
    bool hasOverlay = false;       // NEW: Flag for overlay presence
};
```

#### 2. Update Vertex Format
**File**: `src/graphics/ChunkMesh.h`

```cpp
struct ChunkVertex {
    glm::vec3 Position;
    glm::vec2 UV;
    glm::vec2 OverlayUV;           // NEW: Overlay texture coordinates
    float AO;
};
```

Update bgfx vertex layout:
```cpp
ChunkVertex::VertexLayout()
    .begin()
    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)  // Base UV
    .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)  // NEW: Overlay UV
    .add(bgfx::Attrib::Color0, 1, bgfx::AttribType::Float)     // AO
    .end();
```

#### 3. Detect Overlay in ModelBakery
**File**: `src/resources/ModelBakery.cpp`

Detect overlay textures by:
- Checking if `face.tintindex >= 0` (tinted textures are usually overlays)
- Or checking if texture name contains "overlay"
- Or detecting multiple elements with same face direction

When overlay detected:
```cpp
BakedQuad ModelBakery::BakeFace(...) {
    // ... existing code ...

    // Check if this face is an overlay
    bool isOverlay = (face.tintindex >= 0) ||
                     (face.texture.find("overlay") != std::string::npos);

    if (isOverlay) {
        quad.hasOverlay = true;
        ResolveTextureUVs(face.texture, face.uv, textureAtlas, quad.overlayUVs);
    } else {
        ResolveTextureUVs(face.texture, face.uv, textureAtlas, quad.uvs);
    }

    return quad;
}
```

**Better approach**: Track element index and detect when multiple elements render the same face direction (base + overlay).

#### 4. Update Mesh Generation
**File**: `src/graphics/ChunkMesh.cpp`

```cpp
void ChunkMesh::AddBakedQuad(...) {
    for (int i = 0; i < 4; i++) {
        ChunkVertex vertex;
        vertex.Position = blockWorldPos + quad.vertices[i];
        vertex.UV = quad.uvs[i];
        vertex.OverlayUV = quad.hasOverlay ? quad.overlayUVs[i] : glm::vec2(0.0f);  // NEW
        vertex.AO = quad.aoWeights[i];
        vertices.push_back(vertex);
    }
}
```

#### 5. Update Shaders

**File**: `shader/chunk/varying.def.sc`
```glsl
vec3 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;  // Base texture UV
vec2 a_texcoord1 : TEXCOORD1;  // NEW: Overlay texture UV
float a_color0   : COLOR0;     // AO

vec2 v_uv        : TEXCOORD0;
vec2 v_overlayuv : TEXCOORD1;  // NEW
float v_ao       : COLOR0;
```

**File**: `shader/chunk/vs_chunk.sc`
```glsl
v_uv = a_texcoord0;
v_overlayuv = a_texcoord1;  // NEW: Pass overlay UV
v_ao = a_color0;
```

**File**: `shader/chunk/fs_chunk.sc`
```glsl
SAMPLER2D(s_tex, 0);       // Base texture
SAMPLER2D(s_texOverlay, 1); // NEW: Overlay texture

void main() {
    // Sample base texture
    vec4 baseColor = texture2D(s_tex, v_uv);

    // Sample overlay texture
    vec4 overlayColor = texture2D(s_texOverlay, v_overlayuv);

    // Blend overlay on top of base (alpha blending)
    // If overlay UV is (0,0), overlayColor.a will be 0, no blending
    vec4 finalColor = mix(baseColor, overlayColor, overlayColor.a);

    // Apply AO
    finalColor.rgb *= v_ao;

    gl_FragColor = finalColor;
}
```

**Note**: Need to bind overlay texture atlas to texture slot 1 in rendering code.

#### 6. Bind Overlay Texture in Rendering

**File**: `src/core/Application.cpp` (or wherever chunk rendering happens)

```cpp
// Bind base texture to slot 0
bgfx::setTexture(0, s_tex, blocksAtlas->GetHandle());

// NEW: Bind overlay texture (same atlas) to slot 1
bgfx::setTexture(1, s_texOverlay, blocksAtlas->GetHandle());
```

Since we use the same atlas for both base and overlay textures, bind the same texture to both slots.

---

## Testing & Verification

### Debug Steps

1. **Check Atlas Contents**:
   - Locate `debug_atlas.png` in project root
   - Verify `grass_block_side_overlay.png` is present in atlas
   - Check `debug_atlas_info.txt` for texture list and UV coordinates

2. **Check Logs**:
   - Look for: `"Loaded texture: block/grass_block_side_overlay"`
   - Look for: `"Resolved texture 'block/grass_block_side_overlay' to UV offset..."`
   - Look for: `"Texture '...' NOT FOUND in atlas!"` errors

3. **Verify Model**:
   - Confirm `assets/models/block/grass_block.json` has 2 elements
   - Confirm overlay element uses `#overlay` texture reference
   - Confirm textures section defines `"overlay": "block/grass_block_side_overlay"`

### Expected Behavior After Fix

- Grass block sides show proper layered texture (dirt base + grass overlay)
- Grass block top shows full grass texture
- Grass block bottom shows dirt texture
- Other blocks (stone, dirt, glass) still render correctly
- No performance regression (overlay UVs only add 8 bytes per vertex)

---

## Code Reference Summary

### Key Files & Functions

**Texture Loading**:
- `src/resources/TextureManager.cpp:102` - `ScanTextures()`
- `src/resources/TextureAtlasBuilder.cpp:190` - `LoadPNG()`
- `src/resources/TextureAtlasBuilder.cpp:243` - `PackTextures()`
- `src/resources/TextureManager.cpp:172` - Texture registration loop

**Model Processing**:
- `src/resources/BlockModelLoader.cpp:252` - `ResolveTextures()`
- `src/resources/ModelBakery.cpp:78` - `BakeFace()`
- `src/resources/ModelBakery.cpp:223` - `ResolveTextureUVs()` ⚠️ **Bug location**

**Mesh Generation**:
- `src/graphics/ChunkMesh.cpp:42` - `GenerateMeshData()`
- `src/graphics/ChunkMesh.cpp:95` - Quad rendering loop
- `src/graphics/ChunkMesh.cpp:207` - `AddBakedQuad()`

**Shaders**:
- `shader/chunk/varying.def.sc` - Vertex attributes and varyings
- `shader/chunk/vs_chunk.sc` - Vertex shader
- `shader/chunk/fs_chunk.sc` - Fragment shader

### Data Structures

**TextureInfo** (`src/resources/TextureManager.h:20-42`):
```cpp
struct TextureInfo {
    glm::vec2 uvOffset;     // UV start position [0-1]
    glm::vec2 uvSize;       // UV dimensions [0-1]
    glm::ivec2 gridCoords;  // Grid position (x, y)
    const Graphics::TextureAtlas* atlas;
    bool isAnimated;
};
```

**BakedQuad** (`src/resources/BakedModel.h:18-50`):
```cpp
struct BakedQuad {
    glm::vec3 vertices[4];   // 4 corner positions
    glm::vec2 uvs[4];        // 4 UV coordinates (SINGLE texture)
    glm::vec3 normal;        // Face normal
    std::string cullface;    // Culling direction
    float aoWeights[4];      // AO weights
    bool shade;              // Apply shading
    int tintindex;           // Tint layer (-1 = no tint)
};
```

**ChunkVertex** (`src/graphics/ChunkMesh.h`):
```cpp
struct ChunkVertex {
    glm::vec3 Position;  // World position
    glm::vec2 UV;        // Texture coordinates [0-1]
    float AO;            // Ambient occlusion weight [0-1]
    // TODO: Add glm::vec2 OverlayUV
};
```

---

## Future Enhancements

### Animated Textures
- Detect `.mcmeta` files alongside textures
- Store frame count and animation speed in `TextureInfo`
- Update UVs per frame in shader or CPU-side

### Biome Tinting
- Pass biome grass/foliage color to shader
- Apply tint color when `tintindex >= 0`
- Support different tint types (grass, foliage, water)

### Emissive Textures
- Add emissive texture channel (e.g., redstone lamp glow)
- Blend emissive on top without AO/lighting
- Store in separate atlas or use alpha channel

### Connected Textures (CTM)
- Support OptiFine's Connected Textures Mod format
- Select texture variant based on neighboring blocks
- Requires more complex UV resolution logic

---

## Conclusion

The texture system is well-architected but lacks support for multi-layer rendering required by grass blocks and other advanced block types. The overlay support implementation outlined above maintains the existing architecture while adding the missing functionality for proper Minecraft-compatible block rendering.
