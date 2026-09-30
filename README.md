# YELLOWTEAMGAME

Fresh Project & Portfolio V foundation preserving the reusable Gateware, Vulkan, EnTT, H2B, JSON level-loading, Blender level-loading, INI, window, and input infrastructure. Gameplay and assets are intentionally minimal for Sprint One.

## Prerequisites

- Windows and PowerShell
- CMake 3.25 or newer
- Ninja
- LLVM `clang-cl` (the tested `dev` compiler)
- Vulkan SDK with `VULKAN_SDK` set

## Build and run

Run from the repository root:

```powershell
cmake --preset dev
cmake --build --preset dev
.\scripts\run.ps1
```

The runtime configuration, shaders, and assets are copied beside the executable during every build, including builds after data-only edits. The game can also be launched with `build/dev` as its working directory. CMake writes the clangd compilation database to `build/dev`; the project-local `.clangd` points clangd there.

## Layout

- `src/APP`: window lifecycle
- `src/DRAW`: Vulkan renderer, buffers, and level-loading infrastructure
- `src/GAME`: intentionally generic ECS components and update processing
- `src/UTIL`: shared configuration, timing, and input context
- `shaders`: runtime-compiled HLSL
- `config`: source-controlled project settings
- `assets/levels`: Blender-exported world JSON and the empty `prototype.json` starting world
- `assets/models`: real exported H2B models (currently absent)
- `assets/textures`, `assets/audio`, `assets/ui`: reserved existing roots
- `include`: vendored Gateware, EnTT, JSON, and INI dependencies

## Add visible content

Code defines behavior. Data defines content and tuning. For this scaffold, content selection lives only in `config/game.ini`:

```ini
[Content]
level=assets/levels/prototype.json
models=assets/models
```

1. Create/place meshes in Blender using the team's existing JSON/H2B export tools. Those tools and Blender source files are **not bundled** here; obtain the compatible exporter from the team/course. An OBJ, FBX, or `.blend` file alone cannot be loaded by this application.
2. Put the exported `.h2b` files in `assets/models`. Put the exported level JSON in `assets/levels/prototype.json`, replacing the empty template, or select another JSON filename in `[Content]`.
3. Build and run using the commands above. No C++ edit or CMake asset list edit is required. Edit source assets/config, not the copies in `build/dev`.

The existing loader expects a top-level `entities` array. Each mesh entry has `name`, `is_mesh: true`, `matrix` (16 flattened floats in the loader's Gateware row-major convention), and `bounds` (eight local-space XYZ corners in Blender bounding-box order). Let the compatible exporter produce coordinates/matrices; do not flatten Blender matrices arbitrarily. A loader-compatible placement record looks like this (it requires a real `Crate.h2b`):

```json
{
  "entities": [{
    "name": "Crate",
    "is_mesh": true,
    "matrix": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
    "bounds": [[-1,-1,-1],[-1,-1,1],[-1,1,1],[-1,1,-1],
               [1,-1,-1],[1,-1,1],[1,1,1],[1,1,-1]],
    "gameType": "none",
    "staticCollidable": false
  }]
}
```

Use dot-free base names: `Crate` and Blender duplicate `Crate.001` both resolve to `assets/models/Crate.h2b`. Placement comes from each entry's matrix; translation is floats 12–14. Duplicate/place objects in Blender and re-export to add world objects. Keep `gameType` absent or `"none"` for ordinary visible scenery. The loader groups metadata by H2B filename, so all instances of a base model must agree on static/dynamic flags. `staticCollidable` is optional existing metadata; no collision gameplay is provided.

The fixed preview camera uses `[PreviewCamera]` XYZ in the same INI, looks along +Z with Y up, and defaults to `(0, 2, -10)`. Start with geometry near the origin. This is only a content preview position. Materials use H2B colors and lighting; the current shaders do not sample textures.

The supplied world is deliberately empty: launching displays the clear background and a no-geometry message until real exports are supplied. Missing H2B files are logged to the console and `LevelLoaderLog.txt` and skipped by the existing loader. Invalid/missing world JSON fails startup. Content changes require restarting; there is no hot reload.

## Optional GAME-side model instances

For a runtime instance, include its mesh in the selected exported world with `gameType` set to a non-`"none"` string such as `"renderable"`. This existing flag loads a hidden template; it does not implement gameplay or automatically create a visible object. The template identifier is its exact exported Blender object name, including any `.001` suffix. Static scenery needs no API call.

After startup has loaded the world, GAME code can use:

```cpp
#include "GAME/GameManager.h"
#include "GAME/GameComponents.h"

GW::MATH::GMATRIXF placement;
GW::MATH::GMatrix::IdentityF(placement);
placement.row4.x = 3.0f;
auto object = GAME::CreateRenderable(registry, "Crate", placement);
// Later transform edits are synchronized by the existing GAME update:
registry.get<GAME::Transform>(object).matrix.row4.x = 4.0f;
// When finished (also supported: GAME::ToDestroy):
registry.destroy(object);
```

The helper copies the loaded template's mesh instances and reuses its geometry/materials. Each object owns its copies; deleting one leaves the template and other instances intact. An unavailable identifier throws a descriptive exception. Runtime calls cannot load an arbitrary H2B: it must already be included as a dynamic template in the selected world. There is one loaded world and one model manager; level replacement/unloading is not supported by this scaffold.

## Protected infrastructure

Content work should touch `assets` and `config/game.ini`; behavior belongs in `src/GAME`. Unless a Jira task specifically requires it, do not edit `src/DRAW/VulkanRenderer.cpp`, `src/DRAW/VulkanBuffers.cpp`, `src/DRAW/Level.cpp`, `src/DRAW/DrawComponents.h`, `src/DRAW/Utility` (core loading/H2B internals), `src/APP` (Gateware integration), or `include` (third-party libraries). Teammates do not need to manage GPU instances, buffers, descriptors, or pipelines.
