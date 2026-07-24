# Milestone A — Foundation

Small, solid base for bigger improvements later.

## What we added

### Frame profiler
Toggle in **Options → UI**.  
Shows how long each frame spends in **simulation** vs **drawing** (milliseconds). Useful when large cities feel slow.

### Walker debug
Toggle in **Options → UI**.  
Click any walker to see:
- who they are (`id`, `type`)
- what they are doing (`action`, `wait`)
- where they are going (`pos`, `dest`, buildings)
- path progress and cargo

A red box also highlights the selected walker on the map.

## What we fixed

1. **Cursor vs clicks at high display scale** — clicks land where the cursor is (SDL2).
2. **Delete house → Undo exploit** — population ages are restored correctly; no free workers from re-rolls.
3. **Cart pusher stuck at warehouse** — if the warehouse rejects goods, the walker looks for a new place to deliver.

## Build (Windows)

MSVC + SDL2. Output: `build/RelWithDebInfo/augustus.exe`  
You still need original **Caesar 3 (1.0.1.0)** data files (and Augustus extra assets).

See the main [README](../README.md) for upstream install notes.
