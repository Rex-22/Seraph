# Quick Reference - Minecraft Block Model System

**Last Updated**: 2025-11-11 Session 5
**Status**: 50% Complete - Ready for Phase 8 Integration

## 📁 Documentation Structure

```
.agent/
├── ARCHITECTURE.md        # Complete system design & architecture
├── JSON_SPEC.md          # Minecraft JSON format specification
├── PROGRESS_SUMMARY.md   # Detailed progress tracking
├── INTEGRATION_PLAN.md   # Phase 8 execution plan (THIS IS NEXT)
└── QUICK_REFERENCE.md    # This file
```

## 🎯 Current Status

**What's Complete** (50%):
- ✅ JSON infrastructure (nlohmann/json)
- ✅ All data structures (BlockModel, BakedModel, BlockState)
- ✅ All loaders (BlockModelLoader, ModelBakery, BlockStateLoader, TextureManager)
- ✅ Visual properties (TransparencyType, light emission, AO)
- ✅ Example JSON files (dirt, stone, grass_block, glass, air)
- ✅ Comprehensive documentation

**What's Next** (Phase 8 - Rendering Integration):
- 🔄 Update ChunkMesh to use BakedModel
- 🔄 Add per-vertex AO calculation
- 🔄 Update shaders for AO support
- 🔄 Initialize systems in Application
- 🔄 Test end-to-end

## 📊 Project Overview

### The Pipeline

```
JSON Files                      C++ Data Structures
──────────                      ───────────────────

blockstate.json
    ↓                          BlockState
    ↓ BlockStateLoader    →    ├─ properties
    ↓                          ├─ stateId
    ↓                          └─→ BakedModel
    ↓
model.json
    ↓                          BlockModel
    ↓ BlockModelLoader    →    ├─ parent
    ↓                          ├─ textures
    ↓                          └─ elements
    ↓
    ↓                          BakedModel
    ↓ ModelBakery         →    └─ quads[]
    ↓                             ├─ vertices[4]
    ↓                             ├─ uvs[4]
    ↓                             ├─ normal
    ↓                             ├─ aoWeights[4]
    ↓                             └─ cullface
    ↓
textures/                      TextureAtlas
    ↓ TextureManager      →    ├─ bgfx handle
                               └─ UV mapping
```

### File Locations

**Source Code**:
- `src/resources/` - All loaders, baking, and management (1,500 LOC)
- `src/world/` - BlockState and Block (200 LOC)
- `shader/chunk/` - Chunk rendering shaders

**Assets**:
- `assets/blockstates/` - Block state definitions (5 files)
- `assets/models/block/` - Block models (8 files)
- `assets/textures/block/` - Texture files
- `assets/pack.mcmeta` - Resource pack metadata

## 🔑 Key Classes

### BlockModelLoader
```cpp
// Load and resolve model from JSON
BlockModel* model = loader->LoadModel("block/dirt");
// Handles parent inheritance and texture variables
```

### ModelBakery
```cpp
// Compile model to renderable quads
BakedModel* baked = bakery->BakeModel(*model, textureAtlas);
// Applies rotations, calculates UVs, ready for GPU
```

### BlockStateLoader
```cpp
// Load blockstate and create states
std::vector<BlockState*> states =
    loader->LoadBlockState("dirt", dirtBlock);
// Links blocks to their baked models
```

### TextureManager
```cpp
// Manage atlases and texture lookups
TextureManager* mgr = new TextureManager();
mgr->LoadResourcePack("assets");
TextureAtlas* atlas = mgr->GetAtlas("blocks");
```

## 📋 Next Steps (Execute INTEGRATION_PLAN.md)

### Phase 8 - Day 1 (4 hours)

**Morning (2 hours)**:
1. Update shaders (30 min)
   - Add AO weight to vertex structure
   - Update varying.def.sc, vs_chunk.sc, fs_chunk.sc
2. Update Application init (45 min)
   - Initialize all managers
   - Load blocks from JSON
3. Setup test scene (15 min)
4. Build and verify (30 min)

**Afternoon (2 hours)**:
5. Update ChunkMesh vertex structure (10 min)
6. Add BakedModel iteration (30 min)
7. Add vertex transformation (15 min)
8. Test basic rendering (45 min)
9. Fix UV issues (20 min)

### Phase 8 - Day 2 (3.5 hours)

**Morning (2 hours)**:
10. Implement face culling (20 min)
11. Implement AO calculation (50 min)
12. Test AO visually (30 min)
13. Performance optimization (20 min)

**Afternoon (1.5 hours)**:
14. Run all tests (90 min)
15. Document findings
16. Update TASK.md

## 🚨 Important Notes

### Before Starting Integration:
1. **Read** `.agent/INTEGRATION_PLAN.md` in detail
2. **Create** a git branch for integration work
3. **Backup** ChunkMesh.cpp before modifying
4. **Test** incrementally - don't change everything at once

### Critical Integration Points:
1. **ChunkMesh.cpp** line 40-140: Replace hardcoded faces
2. **Application.cpp** initialization: Add new managers
3. **shader/chunk/** files: Add AO support
4. **ChunkVertex**: Add AO weight field

### Success Criteria:
- [ ] Blocks render with correct textures from JSON
- [ ] Face culling works (no invisible blocks)
- [ ] Ambient occlusion visible
- [ ] No crashes or memory leaks
- [ ] FPS >= 30

### Rollback Plan:
- Keep old ChunkMesh in ChunkMesh_legacy.cpp
- Add compile flag to switch between old/new
- Document all changes in git commits

## 📚 JSON Format Examples

### Blockstate (assets/blockstates/dirt.json)
```json
{
  "variants": {
    "": { "model": "block/dirt" }
  }
}
```

### Model (assets/models/block/dirt.json)
```json
{
  "parent": "block/cube_all",
  "textures": {
    "all": "block/dirt"
  }
}
```

### Base Model (assets/models/block/cube.json)
```json
{
  "ambientocclusion": true,
  "elements": [
    {
      "from": [0, 0, 0],
      "to": [16, 16, 16],
      "faces": {
        "down":  {"texture": "#down",  "cullface": "down"},
        "up":    {"texture": "#up",    "cullface": "up"},
        "north": {"texture": "#north", "cullface": "north"},
        "south": {"texture": "#south", "cullface": "south"},
        "west":  {"texture": "#west",  "cullface": "west"},
        "east":  {"texture": "#east",  "cullface": "east"}
      }
    }
  ]
}
```

## 🎨 Block Properties

### TransparencyType
```cpp
enum class TransparencyType {
    Opaque,       // stone, dirt - can cull faces
    Transparent,  // glass, leaves - binary alpha, needs sorting
    Translucent   // water, ice - partial alpha, no culling
};
```

### Setting Properties
```cpp
block->SetName("glass")
     ->SetTransparencyType(TransparencyType::Transparent)
     ->SetIsOpaque(false)
     ->SetLightEmission(0)
     ->SetAmbientOcclusion(true);
```

## 🔍 Debugging Tips

### Check Model Loading
```cpp
BlockModel* model = loader->LoadModel("block/dirt");
if (!model) {
    Log::Error("Failed to load model");
}
Log::Info("Model has {} elements", model->GetElements().size());
```

### Check Baking
```cpp
BakedModel* baked = bakery->BakeModel(*model, atlas);
Log::Info("Baked model has {} quads", baked->GetQuadCount());
for (const auto& quad : baked->GetQuads()) {
    Log::Info("  Quad: cullface={}", quad.cullface);
}
```

### Check BlockState
```cpp
auto states = stateLoader->LoadBlockState("dirt", block);
Log::Info("Loaded {} states for dirt", states.size());
for (auto* state : states) {
    Log::Info("  State {}: {} properties",
        state->GetStateId(),
        state->GetProperties().size());
}
```

## 📞 Support & References

**Documentation**:
- Architecture: `.agent/ARCHITECTURE.md`
- JSON Spec: `.agent/JSON_SPEC.md`
- Progress: `.agent/PROGRESS_SUMMARY.md`
- Integration: `.agent/INTEGRATION_PLAN.md`
- Task Tracking: `TASK.md`

**External**:
- Minecraft Wiki: https://minecraft.wiki/w/Model
- bgfx: https://bkaradzic.github.io/bgfx/
- nlohmann/json: https://json.nlohmann.me/

**Code Examples**:
- Base models: `assets/models/block/cube*.json`
- Example blocks: `assets/models/block/{dirt,stone,grass_block,glass}.json`
- Blockstates: `assets/blockstates/*.json`

---

**Remember**: All preparation is done. Phase 8 is pure integration work. Take it step by step, test incrementally, and refer to INTEGRATION_PLAN.md for detailed instructions. Good luck! 🚀
