# Transition Strategy: Chunk Storage to BlockState

**Date**: 2025-11-11
**Status**: At Decision Point
**Current Progress**: 52%

## Current Situation

### What's Complete ✅
All core systems are implemented and initialized:
- ✅ BlockModelLoader - Loads JSON models with parent inheritance
- ✅ ModelBakery - Compiles to BakedModel with quads
- ✅ TextureManager - Manages atlases and texture lookups
- ✅ BlockStateLoader - Parses blockstate JSON
- ✅ BlockState - Links Block → BakedModel
- ✅ Shaders - Support per-vertex AO
- ✅ Application - All managers initialized
- ✅ AddBakedQuad - Method to render BakedModel quads

### The Blocker 🚧

**Chunk currently stores**: `Block*` (via BlockId lookup)
**ChunkMesh needs**: `BlockState*` (to get BakedModel)

**Current Chunk Structure** (`src/world/Chunk.h:44`):
```cpp
std::array<BlockId, ChunkVolume> m_Blocks{};
```

**To use BakedModel, we need**:
```cpp
std::array<BlockStateId, ChunkVolume> m_BlockStates{};
```

## Transition Options

### Option A: Full Migration (Clean but Big Change)
**Time**: 3-4 hours
**Risk**: High (touches core world storage)

**Changes Required**:
1. Change Chunk storage from `BlockId` to `BlockStateId`
   - `std::array<BlockStateId, ChunkVolume> m_BlockStates{};`

2. Create BlockState registry in Blocks class
   - `static std::vector<BlockState*> AllBlockStates;`
   - `static BlockState* GetStateById(BlockStateId id);`

3. Update Chunk methods:
   - `SetBlock(pos, BlockState* state)` instead of `Block* block`
   - `BlockState* BlockStateAt(pos)` instead of `Block* BlockAt(pos)`

4. Update all Chunk initialization code:
   - `chunk->SetBlock(pos, Blocks::DirtDefaultState);`
   - Instead of: `chunk->SetBlock(pos, Blocks::Dirt);`

**Pros**:
- Clean architecture, matches Minecraft
- Enables all BakedModel features
- Proper foundation for future features (rotation, properties)

**Cons**:
- Breaks existing chunk generation code
- Requires updating all Block references to BlockState
- Risk of bugs in world generation

---

### Option B: Parallel Systems (Safe Transition)
**Time**: 1-2 hours
**Risk**: Low (no breaking changes)

**Approach**:
1. **Add** BlockState registry alongside existing Block registry:
   ```cpp
   class Blocks {
       static std::unordered_map<std::string, BlockState*> BlockStatesByName;
   };
   ```

2. **Keep** Chunk storing BlockId (unchanged)

3. **Add** helper to get BlockState from Block*:
   ```cpp
   BlockState* GetBlockState(const Block* block) {
       if (!block) return nullptr;
       std::string name = block->GetName();
       return Blocks::BlockStatesByName[name];
   }
   ```

4. **Modify** ChunkMesh::GenerateMeshData:
   ```cpp
   const Block* block = chunk.BlockAt(blockPos);
   BlockState* state = GetBlockState(block);

   if (state && state->GetBakedModel()) {
       // Use BakedModel (NEW CODE PATH)
       const BakedModel* model = state->GetBakedModel();
       for (const auto& quad : model->GetQuads()) {
           if (!ShouldCullQuad(quad, blockPos)) {
               AddBakedQuad(quad, blockPos);
           }
       }
   } else {
       // Use legacy TextureRegion (OLD CODE PATH)
       // ... existing code ...
   }
   ```

**Pros**:
- No breaking changes to Chunk
- Can test both systems side-by-side
- Gradual migration block-by-block
- Easy to debug and compare

**Cons**:
- Temporary mapping overhead
- Dual code paths (more complex)
- Not fully Minecraft-compatible yet

---

### Option C: Demo Mode (Minimal Testing)
**Time**: 30 minutes
**Risk**: Very Low (separate test)

**Approach**:
1. Create `test/TestBakedModelRendering.cpp`
2. Manually create BlockState and BakedModel
3. Render single block to verify pipeline works
4. Don't modify Chunk or existing rendering

**Pros**:
- Zero risk to existing code
- Proves BakedModel system works
- Easy to debug in isolation

**Cons**:
- Doesn't integrate with real chunks
- Not useful for actual gameplay
- Temporary code that will be deleted

---

## Recommendation: Option B (Parallel Systems)

**Rationale**:
1. **Low risk**: Doesn't break existing code
2. **Testable**: Can verify BakedModel works with real chunks
3. **Gradual**: Migrate blocks one at a time
4. **Debuggable**: Can compare old vs new rendering
5. **Foundation**: Sets up for full migration later

**Implementation Plan** (1-2 hours):

### Step 1: Create BlockState Registry (20 min)
```cpp
// In Blocks.h
class Blocks {
public:
    // Existing...
    static BlockState* GetDefaultState(const std::string& blockName);
    static void RegisterBlockState(const std::string& name, BlockState* state);

private:
    static std::unordered_map<std::string, BlockState*> s_DefaultStates;
};
```

### Step 2: Load BlockStates at Startup (30 min)
```cpp
// In Application::Run() or Blocks::RegisterBlocks()
void LoadBlockStates() {
    // For each known block
    std::vector<std::string> blockNames = {
        "air", "dirt", "stone", "grass_block", "glass"
    };

    for (const auto& name : blockNames) {
        Block* block = Blocks::GetByName(name);  // Need to add this lookup
        if (!block) continue;

        auto states = m_BlockStateLoader->LoadBlockState(name, block);
        if (!states.empty()) {
            Blocks::RegisterBlockState(name, states[0]);  // Use first variant
        }
    }
}
```

### Step 3: Modify ChunkMesh (30 min)
```cpp
void ChunkMesh::GenerateMeshData(const Chunk& chunk) {
    // ... existing init code ...

    for (uint32_t i = 0; i < ChunkVolume; ++i) {
        const auto blockPos = BlockPosFromIndex(i);
        const Block* block = chunk.BlockAt(blockPos);

        // Try to get BlockState (NEW)
        BlockState* state = block ? Blocks::GetDefaultState(block->GetName()) : nullptr;

        if (state && state->GetBakedModel()) {
            // NEW CODE PATH: Use BakedModel
            RenderBlockWithBakedModel(state, blockPos, chunk);
        } else {
            // OLD CODE PATH: Use legacy TextureRegion
            RenderBlockLegacy(block, blockPos, chunk, uvSize);
        }
    }
}
```

### Step 4: Test (30 min)
- Verify blocks render correctly
- Compare old vs new rendering
- Test face culling
- Check performance

---

## Decision Needed

**Question**: Which approach should we take?

**A**: Full migration to BlockState storage (3-4 hours, high risk)
**B**: Parallel systems with gradual migration (1-2 hours, low risk) ⭐ **RECOMMENDED**
**C**: Demo mode only (30 min, no real integration)

**Current Blocker**: Waiting for decision on transition approach

---

## Implementation Notes

### If Option B (Recommended):

1. **First**: Add block name lookup to Blocks class
   ```cpp
   static Block* GetByName(const std::string& name);
   ```

2. **Second**: Create BlockState registry in Blocks
   ```cpp
   static std::unordered_map<std::string, BlockState*> s_DefaultStates;
   ```

3. **Third**: Load BlockStates for known blocks in RegisterBlocks()

4. **Fourth**: Update ChunkMesh to try BakedModel first, fall back to legacy

5. **Fifth**: Test and iterate

### Future Migration (Phase 11):
Once Option B is working:
- Change Chunk storage to BlockStateId
- Remove legacy rendering code path
- Remove Block::TextureRegion()
- Remove GrassBlock class
- Full Minecraft-compatible storage

---

## Current Files Modified

**Ready for Integration**:
- ✅ `src/graphics/ChunkMesh.h` - Has AddBakedQuad method
- ✅ `src/graphics/ChunkMesh.cpp` - Has AddBakedQuad implementation
- ✅ `src/core/Application.h/cpp` - All managers initialized
- ✅ `shader/chunk/*` - AO support added

**Needs Modification** (Option B):
- `src/world/Blocks.h/cpp` - Add BlockState registry
- `src/world/Block.h/cpp` - Add name-based lookup
- `src/graphics/ChunkMesh.cpp` - Add dual code path

**Needs Modification** (Option A):
- `src/world/Chunk.h/cpp` - Change storage type
- `src/world/Chunk.cpp` - Update all methods
- `src/core/Application.cpp` - Update chunk initialization
- All chunk generation code

---

## Recommendation

**Proceed with Option B** - Parallel systems with gradual migration.

This provides the best balance of:
- ✅ Low risk (no breaking changes)
- ✅ Testable (can verify BakedModel works)
- ✅ Gradual (migrate blocks incrementally)
- ✅ Foundation (sets up for full migration)

**Next Steps**:
1. Add Block::GetName() support to Blocks registry
2. Create BlockState registry in Blocks class
3. Load BlockStates for known blocks
4. Update ChunkMesh with dual code path
5. Test both paths work correctly

**Estimated Time**: 1-2 hours additional work
