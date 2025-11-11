# Deferred Items Implementation Plan

**Date**: 2025-11-11 Session 8
**Current Progress**: 70%
**Goal**: Complete remaining 30% to reach 100%

## Deferred Items Summary

### Critical Issues (Blocks Core Functionality)

#### 1. **Texture UV Mapping** (HIGH PRIORITY) ⚠️
**Current State**: All textures map to atlas position (0, 0)
**Issue**: ModelBakery doesn't use TextureManager lookup
**Impact**: Only blocks at grid position (0,0) render correctly
**Location**: `src/resources/ModelBakery.cpp:255-259`
**Fix Time**: 30 minutes

**Current Code**:
```cpp
// TODO: Use TextureManager to look up actual texture position
float baseU = 0.0f;
float baseV = 0.0f;
```

**Needed**:
```cpp
// Look up texture in TextureManager
TextureInfo info = textureManager->GetTextureInfo(texturePath);
float baseU = info.uvOffset.x;
float baseV = info.uvOffset.y;
```

#### 2. **Dynamic Per-Vertex AO** (HIGH PRIORITY)
**Current State**: All AO weights = 1.0 (fully lit)
**Issue**: No ambient occlusion visible
**Impact**: Blocks look flat, no depth perception
**Location**: `src/graphics/ChunkMesh.cpp:184`
**Fix Time**: 1-2 hours

**Needed**:
- Implement CalculateVertexAO() function
- Check 3 adjacent blocks per vertex
- Apply Minecraft's AO algorithm

#### 3. **Transparent Block Rendering** (HIGH PRIORITY)
**Current State**: Glass renders opaque
**Issue**: No transparency sorting or blending
**Impact**: Transparent blocks don't look transparent
**Fix Time**: 1-2 hours

**Needed**:
- Sort transparent blocks back-to-front
- Two-pass rendering (opaque then transparent)
- Proper blend state for transparent quads

---

### Important Features (Enhance Functionality)

#### 4. **Blockstate Rotation Application** (MEDIUM)
**Current State**: Rotation values parsed but not applied
**Issue**: Can't rotate blocks (furnaces, logs, etc.)
**Impact**: All blocks face same direction
**Fix Time**: 1 hour

**Needed**:
- Apply x/y rotations to BakedModel
- Implement uvlock support
- Transform vertices and normals

#### 5. **Random Variant Selection** (MEDIUM)
**Current State**: Always uses first variant
**Issue**: No variation in block appearance
**Impact**: Repetitive visuals (e.g., stone variants)
**Fix Time**: 30 minutes

**Needed**:
- Weighted random selection based on variant.weight
- Deterministic per-position (use block coords as seed)

#### 6. **Dynamic Texture Atlas Building** (MEDIUM)
**Current State**: Hardcoded atlas with 4 textures
**Issue**: Can't add new textures easily
**Impact**: Limited to existing textures
**Fix Time**: 2-3 hours

**Needed**:
- TextureAtlasBuilder class
- Texture packing algorithm (rect-pack or shelf)
- Mipmap-safe padding (1-2px border)
- Load textures from files dynamically

---

### Nice to Have (Polish)

#### 7. **Animated Textures** (LOW)
**Status**: Not started (Phase 6)
**Fix Time**: 2-3 hours

#### 8. **Resource Pack System** (LOW)
**Status**: Not started (Phase 10)
**Fix Time**: 1-2 hours

#### 9. **Legacy Code Cleanup** (MEDIUM)
**Status**: Phase 11 - Ready to execute
**Fix Time**: 1 hour

#### 10. **Testing & Documentation** (MEDIUM)
**Status**: Phase 12
**Fix Time**: 2-3 hours

---

## Implementation Priority

### Sprint 1: Critical Fixes (3-4 hours)
**Goal**: Make system fully functional

1. ✅ **Texture UV Lookup** (30 min) - CRITICAL
   - Update ModelBakery to use TextureManager
   - Fix baseU/baseV to use actual texture positions
   - Test grass block shows correct multi-texture

2. ✅ **Dynamic Per-Vertex AO** (1-2 hours) - HIGH
   - Implement CalculateVertexAO()
   - Check 3 neighbors per vertex
   - Apply to all blocks

3. ✅ **Transparent Rendering** (1-2 hours) - HIGH
   - Sort transparent blocks
   - Two-pass rendering
   - Test glass transparency

**Deliverable**: System with correct textures, AO, and transparency

---

### Sprint 2: Enhanced Features (2-3 hours)
**Goal**: Add rotation and variants

4. ✅ **Blockstate Rotations** (1 hour)
   - Apply x/y rotations to baked models
   - Implement uvlock
   - Test rotated blocks

5. ✅ **Random Variants** (30 min)
   - Weighted selection
   - Deterministic per-position

6. ✅ **Dynamic Atlas** (2 hours)
   - TextureAtlasBuilder
   - Packing algorithm
   - Test with many textures

**Deliverable**: Full blockstate feature set

---

### Sprint 3: Cleanup & Polish (3-4 hours)
**Goal**: Production-ready codebase

7. ✅ **Phase 11: Cleanup** (1 hour)
   - Remove GrassBlock class
   - Remove Block::SetTextureRegion()
   - Remove legacy Chunk methods
   - Remove hardcoded texture regions

8. ✅ **Phase 12: Testing** (2-3 hours)
   - Unit tests for loaders
   - Integration tests for pipeline
   - Performance benchmarks
   - Memory leak checks
   - Documentation updates

**Deliverable**: Clean, tested, documented system

---

### Sprint 4: Advanced Features (3-4 hours)
**Goal**: Animations and resource packs

9. ✅ **Animated Textures** (2-3 hours)
   - AnimatedTexture class
   - .mcmeta parsing
   - Frame updates in render loop

10. ✅ **Resource Pack System** (1-2 hours)
    - pack.mcmeta parsing
    - Pack switching
    - Hot-reload (optional)

**Deliverable**: Full Minecraft feature parity

---

## Detailed Implementation Plans

### 1. Texture UV Lookup Fix

**File**: `src/resources/ModelBakery.h`
```cpp
// Add to ModelBakery class:
private:
    const Resources::TextureManager* m_TextureManager = nullptr;

public:
    void SetTextureManager(const Resources::TextureManager* mgr) {
        m_TextureManager = mgr;
    }
```

**File**: `src/resources/ModelBakery.cpp:221-266`
```cpp
void ModelBakery::ResolveTextureUVs(...) {
    // ... existing validation ...

    // NEW: Look up texture in TextureManager
    TextureInfo texInfo;
    if (m_TextureManager && m_TextureManager->HasTexture(texturePath)) {
        texInfo = m_TextureManager->GetTextureInfo(texturePath);
    } else {
        // Fallback: use (0,0) position
        CORE_WARN("Texture not found in atlas: {}", texturePath);
        texInfo.uvOffset = glm::vec2(0, 0);
        texInfo.uvSize = glm::vec2(uvSpriteWidth, uvSpriteHeight);
    }

    // Map element UVs to atlas UVs using texture position
    outUVs[0] = texInfo.uvOffset + glm::vec2(u0, v0) * texInfo.uvSize;
    outUVs[1] = texInfo.uvOffset + glm::vec2(u1, v0) * texInfo.uvSize;
    outUVs[2] = texInfo.uvOffset + glm::vec2(u1, v1) * texInfo.uvSize;
    outUVs[3] = texInfo.uvOffset + glm::vec2(u0, v1) * texInfo.uvSize;
}
```

**File**: `src/core/Application.cpp:142-143`
```cpp
m_ModelBakery = new Resources::ModelBakery();
m_ModelBakery->SetTextureManager(m_TextureManager);  // NEW
```

---

### 2. Dynamic Per-Vertex AO

**File**: `src/graphics/ChunkMesh.h`
```cpp
private:
    float CalculateVertexAO(
        const World::Chunk& chunk,
        const World::BlockPos& blockPos,
        const glm::vec3& normal,
        const glm::vec3& vertexOffset);

    bool IsBlockOpaque(const World::Chunk& chunk, const World::BlockPos& pos);
```

**File**: `src/graphics/ChunkMesh.cpp`
```cpp
float ChunkMesh::CalculateVertexAO(
    const Chunk& chunk, const BlockPos& blockPos,
    const glm::vec3& normal, const glm::vec3& vertexOffset)
{
    // Determine which 3 blocks to check based on face normal and vertex position
    // Example for top face (+Y normal):
    //   Vertex at (0,1,0): check -X, -Z, corner -X-Z
    //   Vertex at (1,1,0): check +X, -Z, corner +X-Z

    // Implementation needed...
}

// In AddBakedQuad(), replace:
vertex.AO = quad.aoWeights[i];  // OLD

// With:
if (model->IsAmbientOcclusion() && block->HasAmbientOcclusion()) {
    vertex.AO = CalculateVertexAO(chunk, blockPos, quad.normal, quad.vertices[i]);
} else {
    vertex.AO = 1.0f;
}
```

---

### 3. Transparent Block Rendering

**File**: `src/graphics/ChunkMesh.h`
```cpp
private:
    std::vector<ChunkVertex> m_OpaqueVertices;
    std::vector<uint16_t> m_OpaqueIndices;
    std::vector<ChunkVertex> m_TransparentVertices;
    std::vector<uint16_t> m_TransparentIndices;
```

**File**: `src/graphics/ChunkMesh.cpp`
```cpp
// In GenerateMeshData():
// Sort quads into opaque vs transparent

// In rendering:
// Pass 1: Render opaque with Z-write
// Pass 2: Sort transparent back-to-front, render with blending
```

---

### 4. Blockstate Rotation

**File**: `src/resources/BlockStateLoader.cpp:180-181`
```cpp
// Replace TODO with:
if (variant.rotationX != 0 || variant.rotationY != 0) {
    bakedModel = ApplyBlockstateRotation(bakedModel, variant);
}
```

**New Method**:
```cpp
BakedModel* ApplyBlockstateRotation(
    const BakedModel* source,
    const BlockStateVariant& variant)
{
    // Create rotated copy of baked model
    // Rotate vertices and normals
    // Handle uvlock if enabled
}
```

---

## Recommended Implementation Order

### Session 8 (Now): Critical Fixes
1. **Texture UV Lookup** (30 min) - MUST DO
2. **Dynamic AO** (1-2 hours) - SHOULD DO
3. **Document progress** (15 min)

**Result**: System with correct textures and depth

### Session 9: Polish & Features
4. **Transparent Rendering** (1-2 hours)
5. **Blockstate Rotations** (1 hour)
6. **Random Variants** (30 min)

**Result**: Full feature set

### Session 10: Cleanup
7. **Phase 11 Cleanup** (1 hour)
8. **Phase 12 Testing** (2 hours)

**Result**: Production-ready system

### Session 11 (Optional): Advanced
9. **Dynamic Atlas Building** (2-3 hours)
10. **Animated Textures** (2-3 hours)
11. **Resource Pack System** (1-2 hours)

**Result**: Full Minecraft parity

---

## Current Blockers

**Critical Blocker**:
- ⚠️ **Texture UV Mapping** - Without this, only textures at grid (0,0) render correctly
  - Dirt, Stone, Grass might not show correct textures
  - MUST FIX before testing

**Major Issues**:
- **No AO** - Blocks look flat
- **No Transparency** - Glass is opaque

**Minor Issues**:
- No rotations
- No random variants
- Limited texture count

---

## Immediate Action Plan

**Right Now** (Session 8):
1. Fix texture UV lookup in ModelBakery ← START HERE
2. Implement dynamic per-vertex AO
3. Test with all 5 blocks
4. Update TASK.md

**Estimated Time**: 2-3 hours
**Target**: System with correct textures and AO

---

## Success Metrics

**Sprint 1 Success**:
- ✅ Grass block shows correct multi-texture (top/side/bottom)
- ✅ All blocks have proper AO (corners darker)
- ✅ No texture mapping errors
- ✅ FPS >= 30

**Sprint 2 Success**:
- ✅ Glass is transparent
- ✅ Rotated blocks work
- ✅ Stone shows random variants

**Sprint 3 Success**:
- ✅ No legacy code remains
- ✅ All tests pass
- ✅ Documentation complete

**Sprint 4 Success**:
- ✅ Animated water/lava
- ✅ Multiple resource packs
- ✅ Hot-reload works

---

## Notes

- Start with texture UV fix - it's critical and quick (30 min)
- AO is important for visuals but not blocking
- Transparency can wait until basic rendering works
- Cleanup phase should happen after all features work
- Testing should be continuous, not just Phase 12

**Recommendation**: Focus on Sprint 1 (critical fixes) now!
