# Block Model & Texture System Architecture

## Overview

This document describes the architecture for Seraph's Minecraft-compatible block model and texture system. The system is designed to match Minecraft's JSON format exactly while integrating with Seraph's existing bgfx-based rendering pipeline.

## System Components

### 1. Resource Loading Pipeline

```
assets/
├── pack.mcmeta                     # Resource pack metadata
├── blockstates/                    # Block state definitions
│   ├── dirt.json
│   ├── grass_block.json
│   └── stone.json
├── models/
│   └── block/                      # Block model definitions
│       ├── cube.json               # Base cube model
│       ├── cube_all.json           # Cube with same texture all sides
│       ├── cube_bottom_top.json    # Cube with different top/bottom
│       ├── dirt.json
│       ├── grass_block.json
│       └── stone.json
└── textures/
    └── block/                      # Individual texture files
        ├── dirt.png
        ├── grass_block_top.png
        ├── grass_block_side.png
        ├── stone.png
        └── water_flow.png.mcmeta  # Animation metadata
```

### 2. Core Classes

#### TextureManager
**Location**: `src/resources/TextureManager.h/cpp`

**Responsibilities**:
- Load individual texture files from `assets/textures/block/`
- Parse `.mcmeta` files for animation data
- Build texture atlases dynamically using TextureAtlasBuilder
- Manage multiple atlases (blocks, items, GUI, etc.)
- Update animated texture frames each frame
- Provide texture lookup by resource name (e.g., "block/dirt")

**Key Methods**:
```cpp
class TextureManager {
public:
    void LoadResourcePack(const std::string& packPath);
    TextureAtlas* GetAtlas(const std::string& atlasName);
    TextureInfo GetTexture(const std::string& resourceName);
    void UpdateAnimations(float deltaTime);

private:
    std::unordered_map<std::string, std::unique_ptr<TextureAtlas>> m_Atlases;
    std::vector<AnimatedTexture> m_AnimatedTextures;
};
```

#### BlockModel
**Location**: `src/resources/BlockModel.h/cpp`

**Responsibilities**:
- Store parsed JSON model data
- Support parent model inheritance
- Define texture variables (#side, #top, etc.)
- Contain list of BlockElements (cuboids)
- Store display transformations

**Data Structure**:
```cpp
struct BlockModel {
    std::string parent;  // Parent model path
    std::unordered_map<std::string, std::string> textures;  // Texture variables
    std::vector<BlockElement> elements;  // Cuboid definitions
    bool ambientOcclusion = true;

    // Resolved after parent inheritance
    bool isResolved = false;
};
```

#### BlockElement
**Location**: `src/resources/BlockElement.h/cpp`

**Responsibilities**:
- Define a single cuboid within a model
- Specify from/to positions in 1/16th block units (0-16)
- Define rotation around axis
- Contain face definitions for each direction

**Data Structure**:
```cpp
struct BlockElement {
    glm::vec3 from;  // [0-16, 0-16, 0-16]
    glm::vec3 to;    // [0-16, 0-16, 0-16]

    struct Rotation {
        glm::vec3 origin;
        glm::vec3 axis;  // [1,0,0], [0,1,0], or [0,0,1]
        float angle;     // -45, -22.5, 0, 22.5, 45
        bool rescale = false;
    };
    std::optional<Rotation> rotation;

    bool shade = true;  // Apply diffuse lighting
    std::unordered_map<std::string, BlockFace> faces;  // "north", "south", etc.
};
```

#### BlockFace
**Location**: `src/resources/BlockFace.h/cpp`

**Responsibilities**:
- Define texture mapping for one face of a cuboid
- Specify UV coordinates [u0, v0, u1, v1] in texture space (0-16)
- Define cullface direction
- Store texture reference and tint index

**Data Structure**:
```cpp
struct BlockFace {
    std::string texture;     // Texture variable reference (#side) or path
    glm::vec4 uv;           // [u0, v0, u1, v1] in 0-16 range
    std::string cullface;   // "north", "south", etc. or empty
    int rotation = 0;       // 0, 90, 180, 270
    int tintindex = -1;     // -1 = no tint, 0+ = tint layer
};
```

#### BlockModelLoader
**Location**: `src/resources/BlockModelLoader.h/cpp`

**Responsibilities**:
- Parse JSON model files
- Resolve parent model inheritance
- Resolve texture variable references (#side → block/grass_block_side)
- Validate model data
- Cache loaded models

**Key Methods**:
```cpp
class BlockModelLoader {
public:
    BlockModel* LoadModel(const std::string& modelPath);

private:
    BlockModel ParseJson(const nlohmann::json& json);
    void ResolveParent(BlockModel& model);
    void ResolveTextures(BlockModel& model);

    std::unordered_map<std::string, std::unique_ptr<BlockModel>> m_ModelCache;
};
```

#### ModelBakery
**Location**: `src/resources/ModelBakery.h/cpp`

**Responsibilities**:
- Compile BlockModel into renderable mesh data (BakedModel)
- Transform model-space coordinates to world-space
- Resolve texture UVs from atlas
- Apply rotations and transforms
- Calculate ambient occlusion weights per vertex

**Data Structure**:
```cpp
struct BakedModel {
    struct BakedQuad {
        glm::vec3 vertices[4];    // World-space positions
        glm::vec2 uvs[4];         // Atlas UV coordinates [0-1]
        glm::vec3 normal;         // Face normal
        std::string cullface;     // Culling direction
        float aoWeights[4];       // Per-vertex AO [0-1]
        bool shade;               // Apply lighting
        int tintindex;            // Tint layer index
    };

    std::vector<BakedQuad> quads;
    bool hasTransparency = false;
    bool isAmbientOcclusion = true;
};

class ModelBakery {
public:
    BakedModel* BakeModel(const BlockModel& model, const TextureManager& texMgr);

private:
    BakedQuad BakeElement(const BlockElement& element, const BlockFace& face);
    glm::vec2 ResolveUV(const std::string& texture, const glm::vec4& uv);
};
```

#### BlockState
**Location**: `src/world/BlockState.h/cpp`

**Responsibilities**:
- Store block state variants (rotation, waterlogged, etc.)
- Map state properties to model selection
- Provide serialization for chunk storage

**Data Structure**:
```cpp
struct BlockState {
    const Block* block;  // Base block type
    uint16_t stateId;    // Unique state ID
    std::unordered_map<std::string, std::string> properties;  // "facing": "north"

    BakedModel* GetModel() const;
};
```

### 3. Data Flow

#### Initialization Flow
```
Application::Init()
  ├─> TextureManager::LoadResourcePack("assets/")
  │     ├─> Parse pack.mcmeta
  │     ├─> Scan textures/block/ for PNG files
  │     ├─> Parse *.png.mcmeta for animation data
  │     ├─> TextureAtlasBuilder::Build(textures)
  │     └─> Create TextureAtlas with bgfx handle
  │
  ├─> BlockModelLoader::LoadBaseModels()
  │     ├─> Load "block/cube.json"
  │     ├─> Load "block/cube_all.json"
  │     └─> Cache parent models
  │
  └─> Blocks::LoadFromJson("assets/blockstates/")
        ├─> For each blockstate JSON:
        │     ├─> Parse variants
        │     ├─> BlockModelLoader::LoadModel(modelPath)
        │     ├─> ModelBakery::BakeModel(model)
        │     └─> Register BlockState
        └─> Build BlockState lookup table
```

#### Rendering Flow
```
ChunkMesh::GenerateMeshData(chunk)
  ├─> For each block position in chunk:
  │     ├─> Get BlockState from chunk data
  │     ├─> Get BakedModel from BlockState
  │     ├─> For each BakedQuad in model:
  │     │     ├─> Check cullface against adjacent blocks
  │     │     ├─> If not culled:
  │     │     │     ├─> Calculate per-vertex AO from neighbors
  │     │     │     ├─> Transform vertices to chunk space
  │     │     │     ├─> Add vertices to buffer
  │     │     │     └─> Add indices to buffer
  │     └─> Next block
  │
  └─> UpdateMesh() → Create bgfx vertex/index buffers

Application::RenderLoop()
  ├─> TextureManager::UpdateAnimations(deltaTime)
  │     └─> Update UV offsets for animated textures
  │
  ├─> For each chunk:
  │     ├─> Bind texture atlas
  │     ├─> Set material uniforms
  │     └─> Draw chunk mesh
  │
  └─> For transparent blocks:
        ├─> Sort by distance
        └─> Draw back-to-front
```

### 4. Coordinate Systems

#### Model Space
- Range: [0, 16] for each axis
- Origin: Corner of block
- Used in JSON element definitions

```
  Y (16)
  ↑
  │     Z
  │    ↗ (16)
  │   /
  │  /
  │ /
  └────────> X (16)
 (0,0,0)
```

#### World Space
- Range: [-∞, +∞]
- Block positions are integers
- Used in chunk mesh generation

#### UV Space
- Range: [0.0, 1.0] normalized coordinates
- Origin: Bottom-left of texture atlas
- Used in shaders

#### Model → World Transformation
```cpp
// Element position in model space [0-16]
glm::vec3 elementPos = glm::vec3(from.x, from.y, from.z);

// Convert to world space [0-1] (one block unit)
glm::vec3 worldPos = elementPos / 16.0f;

// Add block position in chunk
worldPos += glm::vec3(blockPos.X, blockPos.Y, blockPos.Z);
```

#### UV Transformation
```cpp
// Element UV in model space [0-16]
glm::vec4 elementUV = glm::vec4(u0, v0, u1, v1);

// Get texture location in atlas
TextureInfo texInfo = textureManager.GetTexture("block/dirt");

// Convert to atlas UV [0-1]
glm::vec2 uv0 = texInfo.uvOffset + (elementUV.xy / 16.0f) * texInfo.uvSize;
glm::vec2 uv1 = texInfo.uvOffset + (elementUV.zw / 16.0f) * texInfo.uvSize;
```

### 5. Ambient Occlusion Calculation

AO is calculated per-vertex based on the 3 adjacent blocks at each vertex corner:

```
     Side1
      │
   ┌──┼──┐
   │  │  │
───┼──●──┼─── Side2  (● = vertex)
   │  │  │
   └──┼──┘
      │
    Corner
```

**Algorithm**:
```cpp
float CalculateAO(bool side1, bool side2, bool corner) {
    if (side1 && side2) {
        return 0.0f;  // Maximum occlusion
    }
    return (3.0f - (side1 + side2 + corner)) / 3.0f;
}
```

### 6. Transparency Handling

**Block Types**:
1. **Opaque**: No transparency, can cull adjacent faces
2. **Transparent**: Binary alpha (glass), cull faces but needs sorting
3. **Translucent**: Partial alpha (water), no culling, needs sorting

**Rendering Strategy**:
```cpp
// Phase 1: Opaque blocks (front-to-back, early Z-test)
renderOpaqueChunks(state: BGFX_STATE_DEFAULT);

// Phase 2: Transparent blocks (back-to-front, no Z-write)
sortTransparentBlocks(camera);
renderTransparentChunks(state: BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
```

### 7. Performance Optimizations

#### Model Caching
- Cache parsed JSON models to avoid re-parsing
- Cache baked models to avoid re-baking
- Use BlockState ID for fast lookup

#### Mesh Generation
- Only regenerate mesh when chunk changes
- Use dirty flags per chunk
- Batch face additions

#### Texture Atlasing
- Pack all block textures into single atlas
- Minimize texture switches
- Use mipmap-safe padding (1px border)

#### Culling Strategies
1. **Face culling**: Don't generate faces between opaque blocks
2. **Frustum culling**: Don't render chunks outside view
3. **Occlusion culling**: (Future) Don't render chunks behind others

### 8. Error Handling & Fallbacks

**Missing Model**: Use fallback cube model with magenta texture
**Missing Texture**: Use magenta texture (#FF00FF)
**Invalid JSON**: Log error, use fallback, continue loading
**Circular Parent**: Detect and break cycle, log error

**Example**:
```cpp
BlockModel* BlockModelLoader::LoadModel(const std::string& path) {
    try {
        auto json = LoadJsonFile(path);
        return ParseModel(json);
    } catch (const std::exception& e) {
        Log::Error("Failed to load model {}: {}", path, e.what());
        return GetFallbackModel();
    }
}
```

### 9. Extension Points

The system is designed to be extended with:
- **Item models**: Reuse BlockModel for item rendering
- **Entity models**: Different format but similar pipeline
- **Custom model loaders**: Plugin system for mod support
- **Procedural models**: Generate models at runtime
- **LOD models**: Distance-based model simplification

### 10. Memory Management

**Model Data**:
- BlockModel: Heap-allocated, cached, ~1KB per model
- BakedModel: Heap-allocated, cached, ~10KB per model
- Models owned by BlockModelLoader

**Texture Data**:
- CPU texture data freed after atlas upload
- GPU texture memory managed by bgfx
- Atlas size: ~4MB for 256 textures at 16x16

**Mesh Data**:
- Chunk mesh: ~100KB per chunk (compressed)
- Vertex/index buffers on GPU
- Regenerated when chunk changes

## Migration Strategy

### Phase 1: Parallel Systems
- Keep existing hardcoded blocks working
- Add new JSON loading alongside
- Test with new blocks only

### Phase 2: Gradual Migration
- Convert existing blocks to JSON one-by-one
- Dirt → dirt.json
- Grass → grass_block.json

### Phase 3: Cleanup
- Remove hardcoded registration
- Remove Block::SetTextureRegion()
- Remove GrassBlock class (replaced by JSON)

## Testing Strategy

1. **Unit Tests**: JSON parsing, model resolution, UV calculation
2. **Integration Tests**: Full loading pipeline, rendering
3. **Visual Tests**: Screenshot comparison, known-good renders
4. **Performance Tests**: Loading time, mesh generation time, FPS
