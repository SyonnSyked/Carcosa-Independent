# Carcosa

A Windows C++20 game prototype for Project & Portfolio V. The current scene has a player, enemies that chase and attack, a configurable horde spawner, and a brief flash when characters take damage. It uses Gateware for window and input handling, Vulkan for rendering, and EnTT's entity-component system (ECS) to organize gameplay state. Levels come from Blender-exported JSON and H2B models; gameplay settings live in an INI file.

## Prerequisites

- Windows and PowerShell
- CMake 3.25 or newer
- Ninja
- LLVM `clang-cl`
- Visual Studio Build Tools with the C++ tools and Windows SDK available to `clang-cl`
- Vulkan SDK with `VULKAN_SDK` set

## Build and run

Run from the repository root:

```powershell
cmake --preset dev
cmake --build --preset dev
.\scripts\run
```

The run script launches `build/dev/Carcosa.exe`. Each build automatically copies configuration, shaders, and assets beside the executable, including builds after data-only edits. The game resolves those resources from its executable directory regardless of the launch working directory. CMake writes the clangd compilation database to `build/dev`; the project-local `.clangd` points clangd there.

## Official Release submission

```powershell
cmake --preset release
cmake --build --preset release
```

This automatically builds Release, recreates `build/release/distribution`, and creates `build/release/Carcosa.zip` on every Release build, including after data-only edits. The ZIP contains `bin/Carcosa.exe`, redistributable runtime DLLs, `assets/`, `config/`, compiled `shaders/*.spv`, and `ReadMe.txt`. No development or shader source is included. Extract the entire ZIP and launch `bin/Carcosa.exe`; resources resolve from its parent directory. Windows and a Vulkan-capable graphics driver are required; the Vulkan SDK is only needed to build/package. Development builds retain runtime HLSL compilation.

## Layout

- `src/APP`: window lifecycle
- `src/DRAW`: Vulkan renderer, buffers, and level-loading infrastructure
- `src/GAME`: gameplay components, player input and mouse targeting, combat, enemy chase, horde spawning, damage feedback, pickup helpers, camera follow, and update coordination
- `src/UTIL`: shared configuration, timing, and input context
- `shaders`: runtime-compiled HLSL
- `config`: source-controlled project settings
- `assets/levels`: Blender-exported world JSON, including the playable `prototype.json` testbed
- `assets/models`: exported Ground, Player, and Enemy H2B models
- `assets/textures`, `assets/audio`, `assets/ui`: reserved existing roots
- `include`: vendored Gateware, EnTT, JSON, and INI dependencies
- `dropper4blend.py`: Blender scene/model exporter; H2B conversion requires a separate tool

## Add visible content

Code defines behavior; data defines scene content and tuning. Select the level and model directory in `config/game.ini`:

```ini
[Content]
level=assets/levels/Level1.json
models=assets/models
```

1. Save the Blender scene and install/enable the repository's `dropper4blend.py` add-on through Blender's add-on preferences. Author mesh geometry, material colors, and placement in Blender. Use **File > Export > Dropper** to export the world JSON; use JSON for this game's loader.
2. For a model made from multiple mesh pieces, parent the pieces under one root and give the root a string custom property `exportModel`, such as `VampireCleric`. The root can be an Empty or a mesh. Dropper combines its descendant meshes into one model export, writes one JSON placement with combined bounds, and uses `exportModel` as the exported name. Put gameplay metadata such as `gameType` on that root. Do not nest roots that each have `exportModel`; each root must contain at least one mesh.
3. Dropper writes OBJ/MTL files to `Models` beside the saved `.blend` file and looks for `Obj2Header v1.9d.exe` there to convert them to H2B. The converter and Blender source files are **not bundled**; obtain the compatible converter from the team/course. Check that the expected `.h2b` files were produced. An OBJ, FBX, or `.blend` file alone cannot be loaded by this application.
4. Copy the exported `.h2b` files into `assets/models` and the world JSON into `assets/levels`. The current world is `Level1.json`; select another world through `[Content].level` when needed. Keep model names consistent between the JSON, H2B filenames, and the existing character/template settings in `game.ini`.
5. Build and run using the commands above. Adding scenery or replacing an existing model does not require a C++ edit or a CMake asset-list edit. New behavior still belongs in GAME code. Edit source assets/config, not the copies in `build/dev`, and relaunch after content changes.

Meshes without an `exportModel` root still use the individual-object export path. For grouped models, geometry is exported relative to the root while the JSON retains the root's world placement. Dropper restores the authoring transforms after export. Material colors travel through OBJ/MTL conversion into H2B; the current world shaders use those material values and lighting rather than image textures. The exporter supplies geometry and placement, not skeletal animation or gameplay logic.

The existing loader expects a top-level `entities` array. Each mesh entry has `name`, `is_mesh: true`, `matrix` (16 flattened floats in the loader's Gateware row-major convention), and `bounds` (eight local-space XYZ corners in Blender bounding-box order). Let the compatible exporter produce coordinates/matrices; do not flatten Blender matrices arbitrarily. A loader-compatible placement record looks like this (it requires a real `Crate.h2b`):

```json
{
    "entities": [
        {
            "name": "Crate",
            "is_mesh": true,
            "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
            "bounds": [
                [-1, -1, -1],
                [-1, -1, 1],
                [-1, 1, 1],
                [-1, 1, -1],
                [1, -1, -1],
                [1, -1, 1],
                [1, 1, 1],
                [1, 1, -1]
            ],
            "gameType": "none",
            "staticCollidable": false
        }
    ]
}
```

Use dot-free base names: `Crate` and Blender duplicate `Crate.001` both resolve to `assets/models/Crate.h2b`. An `exportModel` root should likewise use a dot-free model name without a filename extension. Placement comes from each entry's matrix; translation is floats 12-14. Duplicate/place objects in Blender and re-export to add world objects. Keep `gameType` absent or `"none"` for ordinary visible scenery. The loader groups metadata by H2B filename, so all instances of a base model must agree on static/dynamic flags. Set `staticCollidable` on static scenery that should also register as an obstacle; it does not assign an enemy or pickup role.

Read the visual-content flow in order:

1. `dropper4blend.py`: export names, custom properties, converted matrices, bounds, and model geometry.
2. `src/DRAW/Utility/load_data_oriented.h`: read the selected JSON and matching H2B models into CPU geometry, materials, and placements. Missing model files are reported through the console and `LevelLoaderLog.txt`.
3. `src/DRAW/Level.cpp`: upload the level's geometry, create visible static instances, and register dynamic models as hidden templates in `DRAW::ModelManager`.
4. `src/GAME/GameManager.cpp`: use `CreateRenderable` to clone those templates for gameplay actors, then synchronize their owned meshes with their `Transform` components during updates.

```text
Blender geometry/materials/placement -> Dropper JSON + OBJ/MTL
                                   -> converter H2B + selected level JSON
                                   -> CPULevel -> GPULevel / ModelManager
                                   -> static scenery or CreateRenderable instances
                                   -> existing Vulkan world rendering
```

World shader sources remain in `shaders/LevelVertex.hlsl` and `shaders/LevelPixel.hlsl`, selected by `[Shaders]` in `game.ini`. Development builds compile HLSL at runtime. The existing Release packaging compiles SPIR-V and adjusts the packaged shader paths automatically; content authors do not need a separate shader-copy step.

## Gameplay reference

During gameplay, the bottom-center HUD shows current/max player health and a health bar, plus Melee (left-click), Axe throw (right-click), and AOE slam (1). Each slot reads its existing attack component to show Ready, remaining cooldown seconds, and a recharge strip. The display scales with the window and does not intercept input. It is hidden on menu screens and when there is no active player. `DrawPlayerHUD` in `src/GAME/GameManager.cpp` controls its layout and colors.

Hold right-click to throw a spinning axe toward the ground under the cursor. Throws repeat every 0.35 seconds, deal 60% of the current melee damage (15 versus the default 25), and travel up to 8 world units at 12 units/second. An axe damages the first living enemy on its path once, then disappears; misses expire. Like existing enemy projectiles, axes currently do not collide with scenery. Melee and key-1 AOE keep their own cooldowns. Restart removes airborne axes and their meshes.

`AxeThrow` in `src/GAME/GameComponents.h` holds tuning; `TryThrowAxe` and `UpdateThrownAxes` in `src/GAME/Combat.cpp` handle creation, movement, spin, and damage. `ThrowingAxe.h2b` is the supplied Quaternius axe, using its original wood, metal, and red material colors, registered as a hidden dynamic template in `assets/levels/Level1.json`. Include that template in any other playable level that supports this attack. The original OBJ, MTL, and CC0 license are preserved in `assets/source/throwing-axe`. The converter centers the model, rotates it onto the X/Z plane, and scales its length to 1.2 game units. To regenerate the model, run `python scripts/generate_throwing_axe.py`; Python is not required to build or play.

Run the axe gameplay checks from a Developer PowerShell: `cmake --preset dev -DCARCOSA_AXE_TESTS=ON`, `cmake --build --preset dev`, then `ctest --test-dir build/dev -R WarriorAxeThrow --output-on-failure`. These use real gameplay code and model data without opening a GPU window.

W/S move along +Z/-Z; A/D move along -X/+X. Movement uses elapsed frame time (delta time), keeps diagonal movement at the same speed, and preserves the exported Y height. Left-click a visible enemy within melee range to attack; holding the button does not repeat. Mouse targeting checks the rendered meshes, so scenery or another actor can block a target. Clicking empty space or an out-of-range enemy does not start the player's cooldown.

Enemies chase and attack automatically. The default horde settings add one enemy every two seconds, 4-7 world units from the player, up to five living enemies at once. Spawning continues as enemies die. Characters briefly flash white when hit, and the console reports hits, health, and deaths. With the default stats, four player hits kill an enemy and ten enemy hits kill the player. Restart the application to reset.

`config/game.ini` contains `[Player]` and `[Enemy]`: exact exported `model` name, `health`, `speed`, `damage`, center-to-center `range` measured in the X/Z plane, and `cooldown` in seconds. `[GameplayCamera]` contains the fixed XYZ offset from the player; the camera follows and looks toward the player. `[PreviewCamera]` only provides its initial position before gameplay initialization.

The JSON marks Player/Enemy as dynamic with `gameType`; Ground remains static. Names, transforms, geometry, and materials go through the existing loader unchanged. Initialization selects the loaded Blender names configured in the INI and uses `CreateRenderable`; unavailable actors/templates fail with a descriptive message.

Recommended reading order:

1. `src/GAME/GameComponents.h`: tags identify roles; components hold state. An entity is an EnTT ID to which components are attached.
2. `src/GAME/Player.cpp`, `UpdatePlayer`: Gateware input moves the player's ECS Transform and selects a clicked enemy using a cursor ray through the scene. `UpdateCamera` follows that transform using the existing DRAW camera.
3. `src/GAME/Combat.cpp`, `TryMeleeAttack`: distance, cooldown, health, logs, and death decisions.
4. `src/GAME/Enemy.cpp`, `UpdateEnemy`: autonomous chase and melee retaliation.
5. `src/GAME/GameManager.cpp`: `InitializeGameplay` attaches configured state to owned renderable instances; `UpdateGameSystems` coordinates timers, player, enemy, camera, existing velocity integration, mesh synchronization, and deferred destruction.
6. Existing `src/DRAW/Level.cpp`, `Destroy_MeshCollection`, and `src/DRAW/VulkanBuffers.cpp`: owned mesh cleanup, camera inversion, and instance upload to Vulkan.

```text
Gateware Input -> GAME systems -> ECS components / Transform
              -> GameManager mesh synchronization -> GPUInstance -> Vulkan
```

A dead enemy receives `ToDestroy`; GameManager destroys it after updates, and the existing MeshCollection callback destroys its visible meshes. A dead player stays visible with zero health; movement, attacks, and enemy activity stop. The camera stays anchored there.

The current prototype has no movement collisions, ground boundaries, pathfinding, animation, HUD, or player respawn. Actors can move beyond the finite Ground mesh. Materials use H2B colors and lighting; the current shaders do not sample textures. Content/config changes require restarting; there is no hot reload. Missing H2B files are logged to the console and `LevelLoaderLog.txt`; missing gameplay actors fail initialization.

Pickup support is partly implemented in `src/GAME/PickUp&Apply.cpp`. `PickUpObject` collects entities with `Transform` and `PickUp` within an X/Z radius, records their bonuses in the player's `BonusStats`, and removes the collected entities. `[Pickup].collectionAllowance` adds to the radius supplied by the caller. The current game loop does not call this helper or create pickups, and movement/combat do not yet apply the recorded bonuses.

## Horde spawner teammate reading guide

The original Enemy and every spawned Enemy use `CreateEnemy(registry, config, placement)` in `src/GAME/GameManager.cpp`. It returns the actor ID, uses `CreateRenderable` for its Transform and owned meshes, attaches the Enemy tag, and copies fresh Health, Movement and MeleeAttack state. It never reads the INI or searches for another enemy's position.

Read this flow in order:

1. `config/game.ini`: `[Player]` and `[Enemy]` provide character recipes; `[HordeSpawner]` provides `enabled`, `spawnInterval` (seconds), `minimumRadius`, `maximumRadius`, `maximumAlive`, and `totalSpawnLimit`. Zero total limit means unlimited; zero interval means at most one spawn per frame. Negative/nonfinite tuning, reversed radii, nonpositive alive caps, negative total limits and missing dynamic models fail startup with a console diagnostic.
2. `src/GAME/GameManager.cpp`, `LoadCharacter` and `InitializeGameplay`: parse and validate once, obtain placement from the selected Blender world, create the initial actors, and store the shared Enemy `CharacterConfig` in the registry context. The Player recipe is used during initialization only.
3. `src/GAME/GameComponents.h`, `CharacterConfig` and `HordeSpawner`: recipes initialize independent gameplay components; the spawner component holds tuning, elapsed time and its own spawn count. A separate spawner entity has Transform but needs no renderable. Its initial transform comes from the configured Enemy's exported placement.
4. `src/GAME/HordeSpawner.cpp`, `UpdateHordeSpawner`: find the living Player's Transform through EnTT, count Enemy entities with positive Health excluding `ToDestroy`, advance timers, and enforce limits. One registry-owned random generator selects an angle and radius around the Player in X/Z. The spawner Transform preserves Enemy orientation, scale and Y height. A saturated timer permits spawning when a cap frees up, without a backlog burst. The original Enemy counts toward the alive cap, but not the spawner's total count. Spawning pauses when the Player dies.
5. `src/GAME/GameManager.cpp`, `CreateEnemy` and `AttachCharacter`: create separate owned renderable instances and ECS state. There is no separate enemy collection or AI object.
6. `src/GAME/Enemy.cpp`, `UpdateEnemy`, and `src/GAME/Combat.cpp`, `TryMeleeAttack`: existing component queries automatically include new Enemy actors. Damage and cooldown state are independent; deaths mark enemies `ToDestroy`.
7. `src/GAME/GameManager.cpp`, `UpdateGameSystems`: spawn after combat so deaths immediately free population slots; new enemies start AI next frame. Synchronize actor Transforms to their owned GPUInstances, then perform deferred destruction. `src/DRAW/Level.cpp`, `Destroy_MeshCollection`, already deletes each actor's owned mesh entities while leaving templates and other actors intact.

```text
INI configuration -> CharacterConfig initialization recipe
                  -> HordeSpawner component -> UpdateHordeSpawner
                  -> CreateEnemy -> independent ECS components
                  -> existing UpdateEnemy / TryMeleeAttack
                  -> existing UpdateGameSystems render synchronization
```

Long frames deliberately discard missed spawn intervals, and each spawner creates at most one enemy per update. There is no collision-aware placement; the ring may extend past Ground. Changing tuning requires restarting the game.

## Damage feedback teammate reading guide

`config/game.ini`, `[DamageFeedback]`, controls `enabled`, `duration` (seconds), and `flashR`, `flashG`, `flashB` (finite RGB channels in [0,1]). The prototype uses a white 0.12-second flash. `InitializeGameplay` calls `InitializeDamageFeedback` in `src/GAME/DamageFeedback.cpp` once, validates the values, and stores `DamageFeedbackConfig` in registry context. Duration must be positive, even when feedback is disabled. Configuration changes require restarting.

`src/GAME/Combat.cpp`, `TryMeleeAttack`, is the shared damage path. When an attack reduces Health, it calls `ActivateDamageFlash` for the victim. Misses and zero damage do not activate the effect. `DamageFlash` in `src/GAME/GameComponents.h` stores only remaining seconds on the actor; subsequent hits reset that time.

`src/GAME/DamageFeedback.cpp`, `SetFlashAppearance`, follows the actor's existing `MeshCollection` and changes only each owned `GPUInstance.matData.Ke` (emissive RGB). A temporary mesh component, `NormalEmission`, saves the original RGB once, so repeated hits cannot overwrite it with the flash color. Shared templates, base colors, other material fields, geometry and GPU resources remain untouched. The existing pixel shader already adds emissive color to lighting: white produces a bright white flash; other colors produce a bright tint over the normal surface.

`UpdateGameSystems` in `src/GAME/GameManager.cpp` calls `UpdateDamageFlash` before combat. This ages previous effects while allowing new hits their full duration. Active flashes keep their emissive color; expiry restores the exact saved RGB and removes both temporary components. A dead Player still updates and returns to its blue appearance. Dead enemies retain the existing deferred destruction policy; their owned meshes and saved RGB state are deleted by `Destroy_MeshCollection`. No saved entity references or resource ownership were added. All HordeSpawner enemies automatically use the same feedback path.

```text
TryMeleeAttack reduces Health -> ActivateDamageFlash -> DamageFlash timer
                             -> UpdateDamageFlash / SetFlashAppearance
                             -> owned GPUInstance emissive RGB
                             -> existing Vulkan upload and LevelPixel.hlsl
                             -> visible damage indication
```

Enemies killed by a hit are removed before the next render, so lethal hits do not keep their models visible just to show a flash. This preserves the existing death behavior. Very short flashes may be missed at low frame rates.

## Runtime model instances

The existing rendering infrastructure supports creating multiple visible instances of a model already included in the exported Blender level.

To make a model available for runtime instantiation, include it in the selected level JSON with `gameType` set to a value other than `"none"` (for example, `"renderable"`). Set the custom property on the Blender object or `exportModel` root before exporting, and include its matching H2B file in the configured models directory.

This marks the model as dynamic and registers its meshes as a hidden template during level loading. It does not automatically create a visible gameplay entity or assign gameplay behavior.

After the world has loaded, student-created GAME systems can create visible instances through the existing helper:

```cpp
#include "GAME/GameManager.h"
#include "GAME/GameComponents.h"

GW::MATH::GMATRIXF placement;
GW::MATH::GMatrix::IdentityF(placement);

placement.row4.x = 3.0f;

auto object = GAME::CreateRenderable(
    registry, "Crate", placement
);
```

In this example, `"Crate"` must match the exact exported Blender object name of an available dynamic template. An arbitrary model filename cannot be supplied to this function.

The returned EnTT entity contains a gameplay `Transform` and an owned `MeshCollection`. GAME systems can subsequently modify its Transform, which the existing gameplay update synchronizes with the visible mesh instances.

Destroying the owning entity also destroys its associated mesh instances through the existing `MeshCollection` cleanup callback, without destroying the original template or other instances.

The Player, Enemy, and Vampire Queen creation paths already use this mechanism, with their model identifiers and initial placement obtained from the existing configuration and exported scene data. Ranged and wizard enemies reuse the Enemy template; `CreateEnemyVariant` attaches their existing attack components and changes only their owned material colors to orange or purple. The wizard's warning area uses the separately loaded `PurpleSpellEffect` template. Merely exporting a model does not attach those behaviors.

Ordinary static scenery does not require GAME-side instantiation; the level loader creates its visible geometry directly.

## ImGui menu presentation

The Main, Pause, Game Over, and Victory screens share a Carcosa theme: dark parchment, antique gold, fine borders, and an original geometric seal. All decoration is drawn through Dear ImGui's existing draw lists. Typography uses the existing ImGui font at different sizes; no menu images, external fonts, or additional rendering framework are required.

Menu presentation comes from required settings in `config/game.ini`:

- `[MenuTheme]`: background, parchment, brass, gold, highlight, normal/muted text, danger accent, and normal/hovered/pressed button colors. Colors use normalized `R,G,B` components in `[0,1]`.
- `[MenuLayout]`: panel width, button width/height/spacing, and title, heading, subtitle, footer, and button font sizes. Dimensions are scaled from the existing 1280x720 reference layout; supported ranges are documented beside the keys.
- `[MainMenu]`, `[PauseMenu]`, `[GameOverMenu]`, and `[VictoryMenu]`: `title`, `subtitle`, `tagline`, `footer`, and the labels for each screen's existing actions.

Edit the existing keys rather than replacing a whole section with a partial example. `LoadMenuSettings` validates every required value at startup; there are no C++ fallback titles, labels, palette colors, or configurable dimensions. Missing settings report their section/key, and invalid colors, dimensions, or labels stop startup with a diagnostic. Rebuild to copy the source INI beside the executable, then relaunch; menu settings are not reread every frame.

Read the menu flow in order:

1. `src/GAME/GameManager.cpp`, `ButtonStyleSetup` and `LoadMenuSettings`: load and validate the INI into cached `MenuSettings` and per-screen `MenuPresentation` data, then prepare menu-local colors.
2. `RenderScreens`: dispatch the current screen to `DrawMainMenu`, `DrawPauseMenu`, `DrawGameOver`, or `DrawWinScreen`.
3. `ScopedMenu`, `BeginMenu`, `DrawMenuFrame`, `DrawMenuSeal`, and `MenuText`: center the composition, draw shared ornamentation, and apply font/style settings. Scope ownership restores the ImGui stacks after each menu; long text is fitted to the available width.
4. `MenuButton` and each screen function: draw native ImGui buttons with the configured labels while keeping the named `MenuAction` and its C++ action separate. Changing a label does not change which action runs.

Main retains Start/Quit; Pause retains Resume/Main Menu/Quit; Game Over and Victory retain Main Menu/Restart. Using the correct dungeon key at the exit opens Victory. Start always creates a fresh run, including after death, victory, or leaving a paused run. Resume preserves the paused run. Start and Restart remove actors and their owned meshes, projectiles, spell areas, keys, exits, pickups, and pending deletions before recreating gameplay. Presentation edits do not change those state transitions or cleanup ownership.

To run the deterministic gameplay/menu regression checks: `cmake --preset dev -DCARCOSA_BUILD_TESTS=ON`, `cmake --build --preset dev`, then `ctest --test-dir build/dev --output-on-failure`. These checks click actual ImGui menu buttons and exercise repeated death/reset, pause/resume, boss/key/victory, transient cleanup, and both Quit actions without opening a GPU window.

```text
game.ini menu sections -> validated MenuSettings / MenuPresentation
                      -> RenderScreens -> scoped menu helpers / native buttons
                      -> existing ImGui frame -> existing Vulkan overlay submission
```

For text, palette, or supported sizing changes, edit the INI. For changes to the code-drawn ornament or composition, use the shared helpers in `GameManager.cpp` rather than duplicating drawing code in every screen. Leave ImGui context/backend setup, frame submission, descriptor pools, and shutdown in the existing application/renderer path.

## Protected infrastructure

Content work should touch `assets` and `config/game.ini`; behavior belongs in `src/GAME`. Unless a Jira task specifically requires it, do not edit `src/DRAW/VulkanRenderer.cpp`, `src/DRAW/VulkanBuffers.cpp`, `src/DRAW/Level.cpp`, `src/DRAW/DrawComponents.h`, `src/DRAW/Utility` (core loading/H2B internals), `src/APP` (Gateware integration), or `include` (third-party libraries). Teammates do not need to manage GPU instances, buffers, descriptors, or pipelines.
