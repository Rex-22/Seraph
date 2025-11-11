# Phase 8: Rendering Integration Plan

**Status**: Ready to Execute
**Dependencies**: Phases 1-7, 9 Complete
**Critical Path**: Yes - blocks all remaining phases

## Overview

This phase integrates the complete JSON-driven block model system with the existing rendering pipeline. All data structures, loaders, and systems are complete. We need to wire them together and test.

## Current State Assessment

### ✅ Complete & Ready:
1. **BlockModelLoader** - Loads and resolves JSON models
2. **ModelBakery** - Compiles models to BakedModel (quads with UVs, normals, AO weights)
3. **TextureManager** - Manages atlases and texture lookups
4. **BlockStateLoader** - Loads blockstates and creates BlockState objects
5. **BlockState** - Links Block → BakedModel
6. **Block** - Enhanced with TransparencyType, light emission, AO flag

### 🔄 Needs Integration:
1. **ChunkMesh** - Currently uses hardcoded cube generation
2. **Chunk** - Currently stores BlockId, could store BlockState ID
3. **Shaders** - Need per-vertex AO support
4. **Application** - Needs to initialize new systems
5. **Blocks registry** - Needs to use new JSON loading

## Integration Tasks

### Task 1: ChunkMesh Integration (CRITICAL)

**File**: `src/graphics/ChunkMesh.cpp`

**Current Approach** (lines 40-140):
```cpp
// Hardcoded cube faces
for each block in chunk:
    get texture region from Block::TextureRegion()
    calculate UVs from atlas
    add 6 faces (hardcoded vertices)
    apply face culling
```

**New Approach**:
```cpp
// Use baked models
for each block in chunk:
    BlockState* state = GetBlockStateForId(blockId)
    BakedModel* model = state->GetBakedModel()
    for each quad in model->GetQuads():
        if shouldCullFace(quad.cullface):
            continue
        transform quad vertices to chunk space
        calculate per-vertex AO
        add to mesh
```

**Implementation Steps**:

1. **Add BakedModel support** (30 min):
   ```cpp
   // In ChunkMesh.h
   #include "resources/BakedModel.h"
   #include "world/BlockState.h"

   // In GenerateMeshData():
   // Replace hardcoded face generation with:
   const BlockState* state = GetBlockState(blockId);
   if (!state) continue;

   const BakedModel* model = state->GetBakedModel();
   if (!model || model->IsEmpty()) continue;

   for (const auto& quad : model->GetQuads()) {
       // Process quad...
   }
   ```

2. **Implement face culling** (20 min):
   ```cpp
   bool ShouldCullQuad(const BakedQuad& quad, const BlockPos& pos) {
       if (quad.cullface.empty()) {
           return false; // No culling
       }

       // Get adjacent block in cullface direction
       BlockPos adjacentPos = GetAdjacentPos(pos, quad.cullface);
       const Block* adjacentBlock = GetBlockAt(adjacentPos);

       // Cull if adjacent block is opaque
       return adjacentBlock && adjacentBlock->IsOpaque();
   }
   ```

3. **Transform vertices to chunk space** (15 min):
   ```cpp
   // Quad vertices are in model space [0-1] (one block unit)
   // Transform to chunk space by adding block position
   for (int i = 0; i < 4; i++) {
       vertex.Position = quad.vertices[i] +
                        glm::vec3(blockPos.X, blockPos.Y, blockPos.Z);
       vertex.UV = quad.uvs[i];
   }
   ```

4. **Update vertex structure** (10 min):
   ```cpp
   struct ChunkVertex {
       glm::vec3 Position;
       glm::vec2 UV;
       float AO;  // NEW: Ambient occlusion weight [0-1]
   };
   ```

**Estimated Time**: 75 minutes

---

### Task 2: Per-Vertex Ambient Occlusion (CRITICAL)

**File**: `src/graphics/ChunkMesh.cpp`

**Algorithm** (Minecraft-style):
For each vertex of a quad, check 3 adjacent blocks:
- side1: block adjacent to first edge
- side2: block adjacent to second edge
- corner: block at diagonal corner

```cpp
float CalculateVertexAO(bool side1, bool side2, bool corner) {
    if (side1 && side2) {
        return 0.0f;  // Maximum occlusion (darkest)
    }
    int occludedCount = side1 + side2 + corner;
    return (3.0f - occludedCount) / 3.0f;  // 0.33, 0.66, or 1.0
}
```

**Implementation Steps**:

1. **Add AO calculation helper** (30 min):
   ```cpp
   float ChunkMesh::CalculateVertexAO(
       const BlockPos& blockPos,
       const glm::vec3& normal,
       const glm::vec3& vertexOffset  // [0-1] relative to block
   ) {
       // Determine which 3 blocks to check based on normal and vertex position
       // For example, for top face (+Y):
       //   Vertex at (0,1,0): check -X, -Z, and corner -X-Z
       //   Vertex at (1,1,0): check +X, -Z, and corner +X-Z
       //   etc.

       // Get the 3 adjacent block positions
       std::vector<BlockPos> checkPositions =
           GetAOCheckPositions(blockPos, normal, vertexOffset);

       bool side1 = IsBlockOpaque(checkPositions[0]);
       bool side2 = IsBlockOpaque(checkPositions[1]);
       bool corner = IsBlockOpaque(checkPositions[2]);

       return CalculateAO(side1, side2, corner);
   }
   ```

2. **Integrate with quad processing** (20 min):
   ```cpp
   for (const auto& quad : model->GetQuads()) {
       // ... existing code ...

       for (int i = 0; i < 4; i++) {
           vertex.Position = quad.vertices[i] + blockPosVec;
           vertex.UV = quad.uvs[i];

           // Calculate AO if model has AO enabled
           if (model->IsAmbientOcclusion() && block->HasAmbientOcclusion()) {
               vertex.AO = CalculateVertexAO(
                   blockPos, quad.normal, quad.vertices[i]
               );
           } else {
               vertex.AO = 1.0f;  // Fully lit
           }
       }
   }
   ```

**Estimated Time**: 50 minutes

---

### Task 3: Shader Updates (CRITICAL)

**Files**:
- `shader/chunk/varying.def.sc`
- `shader/chunk/vs_chunk.sc`
- `shader/chunk/fs_chunk.sc`

**Current Vertex Structure**:
```glsl
vec3 a_position
vec2 a_texcoord0
```

**New Vertex Structure**:
```glsl
vec3 a_position
vec2 a_texcoord0
float a_texcoord1  // AO weight (reusing texcoord1)
```

**Implementation Steps**:

1. **Update varying.def.sc** (5 min):
   ```glsl
   vec3 a_position  : POSITION;
   vec2 a_texcoord0 : TEXCOORD0;
   float a_texcoord1 : TEXCOORD1;  // NEW: AO weight

   vec2 v_texcoord0 : TEXCOORD0;
   float v_ao : TEXCOORD1;  // NEW: Pass AO to fragment shader
   ```

2. **Update vs_chunk.sc** (5 min):
   ```glsl
   $input a_position, a_texcoord0, a_texcoord1
   $output v_texcoord0, v_ao

   void main() {
       v_texcoord0 = a_texcoord0;
       v_ao = a_texcoord1;  // Pass AO weight
       gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
   }
   ```

3. **Update fs_chunk.sc** (10 min):
   ```glsl
   $input v_texcoord0, v_ao

   SAMPLER2D(s_texColor, 0);

   void main() {
       vec4 texelColor = texture2D(s_texColor, v_texcoord0);

       // Apply ambient occlusion
       vec3 finalColor = texelColor.rgb * v_ao;

       gl_FragColor = vec4(finalColor, texelColor.a);
   }
   ```

4. **Update ChunkVertex layout** (10 min):
   ```cpp
   // In ChunkMesh.cpp
   Layout.begin()
       .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
       .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
       .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)  // NEW: AO
       .end();
   ```

**Estimated Time**: 30 minutes

---

### Task 4: Application Initialization (HIGH PRIORITY)

**File**: `src/core/Application.cpp`

**Current Initialization**:
```cpp
// Hardcoded texture atlas
m_Atlas = TextureAtlas::Create("textures/block_sheet.png", 16);

// Hardcoded block registration
Blocks::RegisterBlocks(this);
```

**New Initialization**:
```cpp
// 1. Initialize TextureManager
m_TextureManager = new TextureManager();
m_TextureManager->LoadResourcePack("assets");

// 2. Initialize BlockModelLoader
m_BlockModelLoader = new BlockModelLoader();

// 3. Initialize ModelBakery
m_ModelBakery = new ModelBakery();

// 4. Initialize BlockStateLoader
m_BlockStateLoader = new BlockStateLoader(
    m_BlockModelLoader,
    m_ModelBakery,
    m_TextureManager->GetAtlas("blocks")
);

// 5. Load blocks from JSON
LoadBlocksFromJSON();
```

**Implementation Steps**:

1. **Add member variables** (5 min):
   ```cpp
   // In Application.h
   Resources::TextureManager* m_TextureManager = nullptr;
   Resources::BlockModelLoader* m_BlockModelLoader = nullptr;
   Resources::ModelBakery* m_ModelBakery = nullptr;
   Resources::BlockStateLoader* m_BlockStateLoader = nullptr;
   ```

2. **Implement LoadBlocksFromJSON** (30 min):
   ```cpp
   void Application::LoadBlocksFromJSON() {
       // For each blockstate file in assets/blockstates/
       std::vector<std::string> blockNames = {"air", "dirt", "stone", "grass_block", "glass"};

       for (const auto& name : blockNames) {
           // Create block
           Block* block = new Block(nextBlockId++);
           block->SetName(name);

           // Set properties based on name
           if (name == "air") {
               block->SetTransparencyType(TransparencyType::Opaque);
               block->SetIsOpaque(false);
           } else if (name == "glass") {
               block->SetTransparencyType(TransparencyType::Transparent);
               block->SetIsOpaque(false);
           }

           // Load blockstate and get BlockState objects
           auto states = m_BlockStateLoader->LoadBlockState(name, block);

           // Register block and its states
           Blocks::RegisterBlock(block, states);
       }
   }
   ```

3. **Update cleanup** (10 min):
   ```cpp
   void Application::Cleanup() const {
       delete m_BlockStateLoader;
       delete m_ModelBakery;
       delete m_BlockModelLoader;
       delete m_TextureManager;
       // ... existing cleanup ...
   }
   ```

**Estimated Time**: 45 minutes

---

### Task 5: Testing & Validation (HIGH PRIORITY)

**Test Plan**:

1. **Visual Tests** (30 min):
   - [ ] Dirt block renders with correct texture
   - [ ] Grass block shows different textures on top/sides
   - [ ] Stone block renders correctly
   - [ ] Glass block is transparent
   - [ ] Face culling works (adjacent opaque blocks hide faces)
   - [ ] Ambient occlusion is visible (corners darker)

2. **Performance Tests** (15 min):
   - [ ] Chunk generation time < 5ms per chunk
   - [ ] FPS >= 60 with multiple chunks
   - [ ] Memory usage reasonable

3. **Edge Cases** (20 min):
   - [ ] Air blocks render correctly (empty)
   - [ ] Invalid block IDs handled gracefully
   - [ ] Missing models use fallback
   - [ ] Missing textures show magenta

4. **Integration Tests** (25 min):
   - [ ] Full pipeline: JSON → BakedModel → Rendering works
   - [ ] Model caching works (no reloading)
   - [ ] Texture atlas lookups are correct
   - [ ] Rotation values parsed but not applied (expected)

**Estimated Time**: 90 minutes

---

## Implementation Order

### Phase 1: Foundation (Day 1 - 2 hours)
1. Update shaders (30 min) - No dependencies
2. Update Application init (45 min) - Needed for testing
3. Create test scene with known blocks (15 min)
4. Build and verify compilation (30 min)

### Phase 2: Core Integration (Day 1 - 2 hours)
5. Update ChunkMesh vertex structure (10 min)
6. Add BakedModel iteration (30 min)
7. Add vertex transformation (15 min)
8. Test basic rendering (45 min)
9. Fix UV mapping issues (20 min)

### Phase 3: Advanced Features (Day 2 - 2 hours)
10. Implement face culling (20 min)
11. Implement AO calculation (50 min)
12. Test AO visually (30 min)
13. Performance optimization (20 min)

### Phase 4: Polish & Testing (Day 2 - 1.5 hours)
14. Run all tests (90 min)
15. Document findings
16. Update TASK.md

**Total Estimated Time**: 7.5 hours over 2 days

---

## Risk Assessment

### High Risk Items:
1. **UV Coordinate Mismatch**:
   - Risk: Textures appear wrong
   - Mitigation: Add debug logging for UV values
   - Fallback: Use single-color test textures

2. **Face Culling Bugs**:
   - Risk: Invisible faces or performance issues
   - Mitigation: Test with transparent blocks (glass)
   - Fallback: Disable culling temporarily

3. **AO Calculation Errors**:
   - Risk: Black blocks or flickering
   - Mitigation: Start with simple (no AO), then add
   - Fallback: Set all AO to 1.0

### Medium Risk Items:
1. **Performance Regression**: More complex rendering
2. **Memory Usage**: Baked models in cache
3. **Shader Compilation**: Platform differences

---

## Success Criteria

### Must Have (P0):
- [ ] Blocks render with correct textures from JSON
- [ ] Face culling works
- [ ] No crashes or memory leaks
- [ ] FPS >= 30

### Should Have (P1):
- [ ] Ambient occlusion visible
- [ ] Glass transparency works
- [ ] Performance >= baseline
- [ ] Model caching effective

### Nice to Have (P2):
- [ ] Hot-reload of JSON files
- [ ] Debug visualization of baked models
- [ ] Performance profiling tools

---

## Rollback Plan

If critical issues arise:

1. **Quick Rollback** (30 min):
   - Keep old ChunkMesh code in ChunkMesh_legacy.cpp
   - Add compile flag to switch between old/new
   - Allows rapid testing and comparison

2. **Partial Rollback**:
   - Use new system but disable AO
   - Use new system but disable face culling
   - Identify which component is problematic

3. **Full Rollback**:
   - Revert to git commit before integration
   - Document learnings
   - Plan fixes before retry

---

## Post-Integration Tasks

After Phase 8 is complete:

1. **Phase 10**: Resource Pack System
   - pack.mcmeta parsing
   - Multiple resource pack support
   - Pack switching

2. **Phase 11**: Migration & Cleanup
   - Remove hardcoded block registration (Blocks.cpp)
   - Remove GrassBlock class
   - Remove Block::SetTextureRegion()
   - Clean up legacy code

3. **Phase 12**: Testing & Polish
   - Unit tests for all components
   - Integration test suite
   - Performance benchmarks
   - Documentation updates

---

## Notes

- All preparation work (Phases 1-7, 9) is complete
- This is the final integration step before cleanup
- Success here validates the entire architecture
- Take time to test thoroughly
- Document any issues for future reference
- Consider making a git branch for integration work
