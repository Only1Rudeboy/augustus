# Performance guide (huge cities)

## Built-in profiler

**Options → UI → Show frame profiler** (and FPS).

| Label | Meaning |
|-------|---------|
| **S** | Simulation (ticks, walkers, economy) |
| **D** | Draw (map, sprites, UI) |
| **F** | Frame total (approx S+D) |

### How to read it

- **S ≫ D** → bottleneck is logic (figures, pathfinding, buildings).  
- **D ≫ S** → bottleneck is rendering / zoom / assets.  
- Both high → huge city; lower game speed or simplify layout.

## Known hotspots (code map)

| Area | Path | Notes |
|------|------|--------|
| Figure tick | `figure/action.c` | Loops all figure slots every tick |
| Combat search | `figure/combat.c` | O(n) scans for targets |
| Routing | `map/routing.c` | BFS per route; stats in `total_routes_calculated` |
| House overlays | `widget/city/overlay/other.c` | Diagnose per house when overlay on |
| Sentiment | `city/sentiment.c` | All houses, 2×/month |

## Practical tips (players)

1. Prefer **roadblocks** / gates to cut walker range.  
2. Avoid stacking thousands of walkers on one network.  
3. Use **Global labour** carefully (more seekers).  
4. Lower zoom / disable weather/clouds if draw-bound.  
5. Use walker debug only when investigating (extra UI).  

## Future code work (not yet)

- Spatial buckets for combat / nearby figure queries  
- Dirty pathfinding when only local road changes  
- Skip dead figure slots without dense scans  
- Overlay column caching per house for a few ticks  

Use the profiler **before and after** any performance PR.
