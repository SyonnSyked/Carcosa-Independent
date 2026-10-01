# YELLOWTEAMGAME

Fresh Project & Portfolio V foundation preserving the reusable Gateware, Vulkan, EnTT, H2B, JSON level-loading, Blender level-loading, INI, window, and input infrastructure. The small gameplay reference uses the exported Ground, Player, and Enemy scene.

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
- `src/GAME`: gameplay components, player input, combat, enemy chase, camera follow, and update coordination
- `src/UTIL`: shared configuration, timing, and input context
- `shaders`: runtime-compiled HLSL
- `config`: source-controlled project settings
- `assets/levels`: Blender-exported world JSON, including the playable `prototype.json` testbed
- `assets/models`: exported Ground, Player, and Enemy H2B models
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
2. Put the exported `.h2b` files in `assets/models`. Put the exported level JSON in `assets/levels/prototype.json`, or select another JSON filename in `[Content]`.
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

Use dot-free base names: `Crate` and Blender duplicate `Crate.001` both resolve to `assets/models/Crate.h2b`. Placement comes from each entry's matrix; translation is floats 12â€“14. Duplicate/place objects in Blender and re-export to add world objects. Keep `gameType` absent or `"none"` for ordinary visible scenery. The loader groups metadata by H2B filename, so all instances of a base model must agree on static/dynamic flags. `staticCollidable` is optional existing metadata; no collision gameplay is provided.

## Gameplay reference

W/S move along +Z/-Z; A/D move along -X/+X. Movement uses delta time, normalizes diagonals, and preserves the exported Y height. Press SPACE for each melee swing; holding it does not repeat. A missed swing also uses the cooldown. The enemy chases and attacks automatically. Console output reports successful hits, health, and death. Four player hits kill the enemy; ten enemy hits kill the player. Restart to reset.

`config/game.ini` contains `[Player]` and `[Enemy]`: exact exported `model` name, `health`, `speed`, `damage`, center-to-center `range`, and `cooldown` in seconds. `[GameplayCamera]` contains the fixed XYZ offset from the player. The existing perspective renderer uses this camera; `[PreviewCamera]` only provides its initial position before gameplay initialization.

The JSON marks Player/Enemy as dynamic with `gameType`; Ground remains static. Names, transforms, geometry, and materials go through the existing loader unchanged. Initialization selects the loaded Blender names configured in the INI and uses `CreateRenderable`; unavailable actors/templates fail with a descriptive message.

Recommended reading order:

1. `src/GAME/GameComponents.h`: tags identify roles; components hold state. An entity is an EnTT ID to which components are attached.
2. `src/GAME/Player.cpp`, `UpdatePlayer`: Gateware context input changes the player's ECS Transform. `UpdateCamera` follows that transform using the existing DRAW camera.
3. `src/GAME/Combat.cpp`, `TryMeleeAttack`: distance, cooldown, health, logs, and death decisions.
4. `src/GAME/Enemy.cpp`, `UpdateEnemy`: autonomous chase and melee retaliation.
5. `src/GAME/GameManager.cpp`: `InitializeGameplay` attaches configured state to owned renderable instances; `UpdateGameSystems` coordinates timers, player, enemy, camera, existing velocity integration, mesh synchronization, and deferred destruction.
6. Existing `src/DRAW/Level.cpp`, `Destroy_MeshCollection`, and `src/DRAW/VulkanBuffers.cpp`: owned mesh cleanup, camera inversion, and instance upload to Vulkan.

```text
Gateware Input -> GAME systems -> ECS components / Transform
              -> GameManager mesh synchronization -> GPUInstance -> Vulkan
```

A dead enemy receives `ToDestroy`; GameManager destroys it after updates, and the existing MeshCollection callback destroys its visible meshes. A dead player stays visible with zero health; movement, attacks, and enemy activity stop. The camera stays anchored there.

This reference deliberately has no collisions, ground boundaries, pathfinding, animation, HUD, or respawn. Actors can move beyond the finite Ground mesh. Materials use H2B colors and lighting; the current shaders do not sample textures. Content/config changes require restarting; there is no hot reload. Missing H2B files are logged to the console and `LevelLoaderLog.txt`; missing gameplay actors fail initialization.

## Optional GAME-side model instances

For a runtime instance, include its mesh in the selected exported world with `gameType` set to a non-`"none"` string such as `"renderable"`. This existing flag loads a hidden template; it does not automatically create a visible object. `GAME::InitializeGameplay` creates the configured Player and Enemy instances after loading. The template identifier is its exact exported Blender object name, including any `.001` suffix. Static scenery needs no API call.

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
