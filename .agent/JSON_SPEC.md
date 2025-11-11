# Minecraft JSON Format Specification

This document provides the complete specification for Minecraft's block model and texture JSON formats that Seraph will implement.

## Resource Pack Structure

```
assets/
├── pack.mcmeta                 # Pack metadata
├── blockstates/                # Block state definitions
│   └── <block_name>.json
├── models/
│   ├── block/                  # Block models
│   │   └── <model_name>.json
│   └── item/                   # Item models (future)
│       └── <model_name>.json
└── textures/
    ├── block/                  # Block textures
    │   ├── <texture_name>.png
    │   └── <texture_name>.png.mcmeta  # Animation metadata
    └── item/                   # Item textures (future)
        └── <texture_name>.png
```

## 1. pack.mcmeta

Defines resource pack metadata.

### Format

```json
{
  "pack": {
    "pack_format": <integer>,
    "description": <string>
  }
}
```

### Example

```json
{
  "pack": {
    "pack_format": 1,
    "description": "Seraph Default Resource Pack"
  }
}
```

### Fields

- `pack_format`: Integer version number (1 for Seraph)
- `description`: Human-readable pack description

---

## 2. Blockstate JSON

Defines which model to use for a block based on its state.

### Location

`assets/blockstates/<block_name>.json`

### Format

```json
{
  "variants": {
    "<property1>=<value1>,<property2>=<value2>": {
      "model": "<model_path>",
      "x": <rotation>,
      "y": <rotation>,
      "uvlock": <boolean>,
      "weight": <integer>
    }
  }
}
```

### Examples

#### Simple Block (No Properties)

```json
{
  "variants": {
    "": { "model": "block/dirt" }
  }
}
```

#### Block with Rotation

```json
{
  "variants": {
    "facing=north": { "model": "block/furnace" },
    "facing=south": { "model": "block/furnace", "y": 180 },
    "facing=east":  { "model": "block/furnace", "y": 90 },
    "facing=west":  { "model": "block/furnace", "y": 270 }
  }
}
```

#### Block with Random Variants

```json
{
  "variants": {
    "": [
      { "model": "block/stone" },
      { "model": "block/stone_variant1", "weight": 2 },
      { "model": "block/stone_variant2" }
    ]
  }
}
```

### Fields

- `variants`: Object mapping state properties to model references
  - Key: Property string in format `"prop1=val1,prop2=val2"` (empty string for no properties)
  - Value: Model reference object or array of objects for random selection
    - `model`: Model path (string, required)
    - `x`: X-axis rotation in degrees (0, 90, 180, 270)
    - `y`: Y-axis rotation in degrees (0, 90, 180, 270)
    - `uvlock`: Lock UVs when rotating (boolean, default: false)
    - `weight`: Selection weight for random variants (integer, default: 1)

---

## 3. Block Model JSON

Defines the 3D model geometry and textures for a block.

### Location

`assets/models/block/<model_name>.json`

### Format

```json
{
  "parent": "<parent_model_path>",
  "ambientocclusion": <boolean>,
  "display": { ... },
  "textures": {
    "<variable>": "<texture_path_or_variable>"
  },
  "elements": [
    {
      "from": [<x>, <y>, <z>],
      "to": [<x>, <y>, <z>],
      "rotation": { ... },
      "shade": <boolean>,
      "faces": {
        "<direction>": {
          "texture": "<variable>",
          "uv": [<u0>, <v0>, <u1>, <v1>],
          "cullface": "<direction>",
          "rotation": <rotation>,
          "tintindex": <integer>
        }
      }
    }
  ]
}
```

### Example: Full Cube (base model)

```json
{
  "ambientocclusion": true,
  "textures": {
    "particle": "#all"
  },
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

### Example: Cube with Same Texture All Sides

```json
{
  "parent": "block/cube",
  "textures": {
    "down":  "#all",
    "up":    "#all",
    "north": "#all",
    "south": "#all",
    "west":  "#all",
    "east":  "#all",
    "particle": "#all"
  }
}
```

### Example: Grass Block

```json
{
  "parent": "block/cube_bottom_top",
  "textures": {
    "bottom": "block/dirt",
    "top": "block/grass_block_top",
    "side": "block/grass_block_side",
    "particle": "block/dirt"
  }
}
```

### Example: Slab (Half Block)

```json
{
  "parent": "block/cube",
  "textures": {
    "particle": "#side"
  },
  "elements": [
    {
      "from": [0, 0, 0],
      "to": [16, 8, 16],
      "faces": {
        "down":  {"texture": "#bottom", "cullface": "down"},
        "up":    {"texture": "#top"},
        "north": {"texture": "#side", "cullface": "north"},
        "south": {"texture": "#side", "cullface": "south"},
        "west":  {"texture": "#side", "cullface": "west"},
        "east":  {"texture": "#side", "cullface": "east"}
      }
    }
  ]
}
```

### Example: Rotated Element

```json
{
  "elements": [
    {
      "from": [4, 0, 4],
      "to": [12, 16, 12],
      "rotation": {
        "origin": [8, 8, 8],
        "axis": "y",
        "angle": 45,
        "rescale": false
      },
      "faces": { ... }
    }
  ]
}
```

### Top-Level Fields

- `parent`: Parent model to inherit from (string, optional)
- `ambientocclusion`: Enable AO calculation (boolean, default: true)
- `display`: Display transformations for different contexts (object, optional)
- `textures`: Texture variable definitions (object, optional)
- `elements`: Array of cuboid elements (array, required if no parent)

### Texture Variables

Texture variables use the format `#<variable_name>` and can reference:
1. Actual texture paths: `"side": "block/grass_block_side"`
2. Other variables: `"particle": "#side"`

Special variable:
- `particle`: Texture used for break particles

### Elements

Each element defines a cuboid with:

#### `from` (required)
- Array of 3 numbers [x, y, z]
- Position of one corner in model space (0-16)
- Example: `[0, 0, 0]`

#### `to` (required)
- Array of 3 numbers [x, y, z]
- Position of opposite corner in model space (0-16)
- Example: `[16, 16, 16]`

#### `rotation` (optional)
Rotation of the element around a point:
- `origin`: Center point [x, y, z] (default: [8, 8, 8])
- `axis`: Rotation axis - "x", "y", or "z" (required)
- `angle`: Rotation angle - -45, -22.5, 0, 22.5, 45 (required)
- `rescale`: Stretch element back to original size (boolean, default: false)

#### `shade` (optional)
- Apply diffuse shading (boolean, default: true)
- False for emissive blocks (lamps, etc.)

#### `faces` (required)
Object with keys: "down", "up", "north", "south", "west", "east"

Each face can have:

##### `texture` (required)
- Texture variable reference (string)
- Format: `#<variable_name>`
- Example: `"#side"`

##### `uv` (optional)
- UV coordinates in texture space [u0, v0, u1, v1]
- Range: 0-16 (maps to texture pixels)
- Default: Face bounds projected onto texture
- Example: `[0, 0, 16, 16]`

##### `cullface` (optional)
- Direction to cull against (string)
- Values: "down", "up", "north", "south", "west", "east"
- If adjacent block is opaque on this side, don't render face
- Example: `"north"`

##### `rotation` (optional)
- Rotate texture clockwise in 90° increments (integer)
- Values: 0, 90, 180, 270
- Default: 0

##### `tintindex` (optional)
- Tint layer index (integer)
- -1: No tint (default)
- 0+: Apply biome/block-specific tint
- Example: 0 for grass color tinting

### Face Directions

```
       up (Y+)
         │
         │
         │
    north│
    (Z-) │
         └─────── east (X+)
        /
       /
      / south (Z+)
     /
  down (Y-)

west (X-)
```

---

## 4. Texture Animation Metadata

Defines animation properties for a texture.

### Location

`assets/textures/block/<texture_name>.png.mcmeta`

### Format

```json
{
  "animation": {
    "interpolate": <boolean>,
    "width": <integer>,
    "height": <integer>,
    "frametime": <integer>,
    "frames": [<frame_indices>]
  }
}
```

### Examples

#### Simple Loop Animation

```json
{
  "animation": {}
}
```
- Auto-detects frames from image height
- Each frame shown for 1 tick (50ms)

#### Custom Frame Time

```json
{
  "animation": {
    "frametime": 2
  }
}
```
- Each frame shown for 2 ticks (100ms)

#### Custom Frame Order

```json
{
  "animation": {
    "frametime": 2,
    "frames": [0, 1, 2, 3, 2, 1]
  }
}
```
- Custom sequence with palindrome effect

#### Complex Frame Timing

```json
{
  "animation": {
    "frames": [
      0,
      1,
      2,
      {"index": 3, "time": 10},
      2,
      1
    ]
  }
}
```
- Frame 3 held for 10 ticks, others for 1 tick

### Fields

- `interpolate`: Interpolate between frames (boolean, default: false)
- `width`: Frame width in pixels (integer, default: texture width)
- `height`: Frame height in pixels (integer, default: texture width for square frames)
- `frametime`: Default time per frame in ticks (integer, default: 1)
- `frames`: Frame sequence (array, default: [0, 1, 2, ...])
  - Can be array of integers (frame indices)
  - Can be array of objects with `index` and `time` properties

### Frame Layout

Animated textures are stored as vertical strips:

```
┌────────┐
│ Frame 0│  ← First frame (top)
├────────┤
│ Frame 1│
├────────┤
│ Frame 2│
├────────┤
│ Frame 3│
└────────┘
```

Example: 16x64 texture = 4 frames of 16x16

---

## 5. Common Parent Models

These base models should be created first as they're frequently referenced:

### `block/cube.json`
Full cube with separate textures for each face.

### `block/cube_all.json`
Cube with same texture on all faces.
```json
{
  "parent": "block/cube",
  "textures": {
    "down": "#all",
    "up": "#all",
    "north": "#all",
    "south": "#all",
    "west": "#all",
    "east": "#all",
    "particle": "#all"
  }
}
```

### `block/cube_bottom_top.json`
Cube with different top/bottom and uniform sides.
```json
{
  "parent": "block/cube",
  "textures": {
    "down": "#bottom",
    "up": "#top",
    "north": "#side",
    "south": "#side",
    "west": "#side",
    "east": "#side",
    "particle": "#side"
  }
}
```

### `block/cube_column.json`
Cube with different top/bottom and uniform sides (alias).

---

## 6. Texture Path Resolution

Texture paths in models can be:

1. **Absolute**: `"block/dirt"` → `assets/textures/block/dirt.png`
2. **Variable**: `"#side"` → Resolved from textures object
3. **Inherited**: Resolved from parent model

Resolution order:
1. Check current model's textures object
2. If variable reference (#var), look up variable
3. If still a variable, check parent model
4. Repeat until concrete path found or error

Example:
```
Model: grass_block.json
  "textures": {
    "side": "block/grass_block_side"
  }
  "parent": "block/cube_bottom_top"

Parent: cube_bottom_top.json
  "textures": {
    "north": "#side"
  }

Resolution:
  #side → "block/grass_block_side"
  Final: assets/textures/block/grass_block_side.png
```

---

## 7. Model Inheritance Rules

When a model has a parent:

1. **Textures**: Merged (child overrides parent)
2. **Elements**: Child replaces parent (not merged)
3. **ambientocclusion**: Child overrides parent
4. **display**: Merged (child overrides parent)

Example:
```
Parent:
  "textures": {"side": "#all", "top": "#all"}
  "elements": [...]

Child:
  "parent": "parent"
  "textures": {"top": "block/special"}

Result:
  "textures": {"side": "#all", "top": "block/special"}
  "elements": [...] (from parent)
```

---

## 8. Coordinate System Reference

### Model Space
- Origin: [0, 0, 0] at corner
- Range: 0-16 for full block
- Units: 1 = 1/16th of a block

### Face Directions
- **down**: -Y (bottom)
- **up**: +Y (top)
- **north**: -Z (back)
- **south**: +Z (front)
- **west**: -X (left)
- **east**: +X (right)

### UV Coordinates
- Origin: [0, 0] at top-left of texture
- Range: 0-16 (texture pixels)
- [u0, v0]: Top-left corner
- [u1, v1]: Bottom-right corner

---

## 9. Validation Rules

### Model Files
- Must have either `parent` or `elements`
- Element `from` must be < `to` on all axes
- Rotation `angle` must be -45, -22.5, 0, 22.5, or 45
- Rotation `axis` must be "x", "y", or "z"
- Face `rotation` must be 0, 90, 180, or 270
- UV coordinates must be in range 0-16
- Element positions must be in range 0-16

### Texture Variables
- Must start with `#` for variables
- No circular references allowed
- Must resolve to concrete path eventually

### Animation
- Frame indices must be valid for image height
- `frametime` must be positive integer
- Custom frame times must be positive integers

---

## 10. Error Handling

### Missing Files
- Model not found → Use fallback model (magenta cube)
- Texture not found → Use missing texture (magenta/black checkerboard)
- Parent not found → Treat as if no parent

### Invalid JSON
- Parse error → Log error, use fallback
- Invalid field values → Use defaults, log warning
- Circular parent reference → Break cycle, log error

### Runtime Errors
- Unresolved texture variable → Use missing texture
- Invalid UV range → Clamp to 0-16
- Invalid rotation angle → Use 0

---

## 11. Performance Considerations

### Caching
- Cache parsed JSON files
- Cache resolved models (after parent inheritance)
- Cache baked models (compiled to mesh)

### Texture Atlas
- Pack all textures into atlas at load time
- Use mipmap-safe padding (1px border)
- Resolve UVs to atlas coordinates when baking

### Model Baking
- Compile models to optimized mesh data
- Apply rotations and transforms
- Resolve all texture variables
- Calculate AO weights
- Store in efficient format for rendering

---

## References

- Minecraft Wiki: https://minecraft.wiki/w/Model
- Minecraft Wiki: https://minecraft.wiki/w/Resource_pack
- Blockbench: https://www.blockbench.net/ (Model editor)
