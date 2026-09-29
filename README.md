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

The runtime configuration and shaders are copied beside the executable during each build, so the game can also be launched directly from `build/dev`. CMake writes the clangd compilation database to `build/dev`; the project-local `.clangd` points clangd there.

## Layout

- `src/APP`: window lifecycle
- `src/DRAW`: Vulkan renderer, buffers, and level-loading infrastructure
- `src/GAME`: intentionally generic ECS components and update processing
- `src/UTIL`: shared configuration, timing, and input context
- `shaders`: runtime-compiled HLSL
- `config`: source-controlled project settings
- `assets`: empty Sprint One model, level, texture, audio, and UI roots
- `include`: vendored Gateware, EnTT, JSON, and INI dependencies

Add the first real level and gameplay systems only after the team establishes the game design and Blender gameplay metadata contract.
