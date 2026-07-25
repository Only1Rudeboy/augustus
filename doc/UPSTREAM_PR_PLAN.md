# Upstream PR plan (Keriew/augustus)

Split this fork into small, reviewable PRs. Prefer **logic-only** first, then UI, then optional configs.

## Suggested order

| PR | Topic | Main files | Risk |
|----|--------|------------|------|
| 1 | Cartpusher / warehouseman reject + partial delivery | `figuretype/cartpusher.c` | Low |
| 2 | Docker import re-route, cargo return to ship, export slots | `figuretype/docker.c` | Low |
| 3 | Depot + supplier re-route | `figuretype/depot.c`, `supplier.c` | Low |
| 4 | Mid-route storage reject for farms | `cartpusher.c` ACTION_21/22 | Low |
| 5 | Native trader leave on failed remove | `figuretype/trader.c` | Low |
| 6 | House evolve_text_id clobber + diagnose API | `house_evolution.c` | Low |
| 7 | House checklist + heatmaps UI | `window/building/house.c`, overlays | Low |
| 8 | Storage recount + status UI | `warehouse.c`, `distribution.c` | Low |
| 9 | Tax/wage mood anti-cheese | `finance.c`, `sentiment.c` | Medium (design) |
| 10 | Cursor display scale (SDL2) | `platform/SDL2/*` | Low |
| 11 | Undo house age restore | `population.c`, `undo.c` | Medium |
| 12 | Dev tools: profiler + walker debug | `game/profiler.c`, figures UI | Low |
| 13 | Blueprint stamp (optional feature) | `building/blueprint.c` | Medium |
| 14 | Optional configs (50% staff, build over walkers) | config + construction | Low if off by default |

## Rules for each PR

- One concern per PR  
- English strings for new `TR_*` keys  
- No save format change unless unavoidable  
- Gameplay rule changes behind `CONFIG_*` default **off**  
- Manual test notes in PR body  

## Do not bundle

- Blueprint + combat AI  
- Performance rewrite + UI  
- Multiplayer / engine changes  
