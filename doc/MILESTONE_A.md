# Milestone A — Foundation (Rudeboy / Augustus fork)

Delivered on the `Only1Rudeboy/augustus` fork. Goal: **build green**, **dev tooling**, and **top bug fixes** so further gameplay work is safe and measurable.

## What’s in

### 1. Windows MSVC build
- SDL2 2.32.10 + SDL2_mixer 2.8.2 under `ext/SDL2/`
- Configure: Visual Studio 17 2022 / x64
- Output: `build/RelWithDebInfo/augustus.exe`

```powershell
$env:COMPILER = "msvc"
# SDL already extracted to ext/SDL2/
cmake -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="ext/SDL2/SDL2-2.32.10;ext/SDL2/SDL2_mixer-2.8.2" `
  -DAV1_VIDEO_SUPPORT=OFF -B build
cmake --build build -j 4 --config RelWithDebInfo
```

You still need original **Caesar 3 1.0.1.0** data + Augustus assets pack to run.

### 2. Frame profiler
- Config: **Options → UI → “Show frame profiler (sim/draw ms)”** (`ui_display_profiler`)
- Also enables/works with FPS display
- Top-left: FPS + average **S:** sim ms, **D:** draw ms, **F:** frame ms (1s rolling average)
- Code: `src/game/profiler.c`, hooked from SDL2/SDL3 main loop

### 3. Walker debug overlay (figure panel)
- Config: **“Show walker debug info (destination, path, wait)”** (`ui_walker_debug`)
- Click a walker → red debug lines:
  - `id`, `type`, `act` (action_state), `wait`
  - `pos`, `dest`, source/dest building ids
  - path progress, road flag, resource, loads

### 4. Bug fixes

| Issue | Fix |
|-------|-----|
| **#1226** Hardware cursor vs click position at display scale ≠ 100% | SDL2: convert window→logical via `SDL_RenderWindowToLogical` (same idea as SDL3). |
| **#953** Free workers via delete house + Undo (age re-roll) | Clear-land house eviction uses tracked age removal; undo restores exact ages. |
| **#951** Cartpusher stuck when warehouse stops accepting | On failed delivery, clear destination and re-run destination search; `should_change_destination` no longer keeps dead targets for workshop/default states. |

## How to use day-to-day

1. Enable **profiler** while developing large cities → see if you’re sim-bound or draw-bound.
2. Enable **walker debug**, click stuck cart pushers → read `act` / `dest` / `path`.
3. Prefer config flags over hardcoded cheats for fork features.

## Files touched (high level)

- `src/game/profiler.c` / `profiler.h` (new)
- `src/game/game.c`, `game.h`
- `src/platform/SDL2/augustus.c`, `renderer.c`
- `src/platform/SDL3/augustus.c`, `renderer.c`
- `src/platform/renderer.h`
- `src/core/config.c` / `config.h`
- `src/window/config.c`, `city.c`
- `src/window/building/figures.c`
- `src/figuretype/cartpusher.c`, `migrant.c` / `migrant.h`
- `src/building/construction_clear.c`, `house.c`
- `src/city/population.c` / `population.h`
- `src/game/undo.c`
- `src/translation/translation.h`, `english.c`
- `CMakeLists.txt`

## Out of scope (Milestone B+)

- Full path-line overlay on the map tiles
- Housing “why not evolve” diagnose
- Blueprint / copy-paste
- Deeper pathfinding rewrite

## Next (Milestone B)

See conversation roadmap: housing diagnose, heatmaps, blueprints.
