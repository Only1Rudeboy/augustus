# Fork summary (master)

This fork builds on [Keriew/augustus](https://github.com/Keriew/augustus).  
Everything listed here is merged into **`master`**.

## Goals

1. Make large cities easier to understand (info, not guesswork).  
2. Fix logistics and fairness bugs that feel like “the game is broken.”  
3. Give developers light tools (profiler, walker debug).  
4. Keep save files compatible — no format breaks.  
5. Gate optional rule changes behind config (default off).

---

## Tools (Options → UI / hotkeys)

| Option / key | What it does |
|--------------|----------------|
| Show frame profiler | FPS + average sim / draw / frame ms |
| Show walker debug info | Click walker: id, action, destination, path, wait, cargo |
| Show house upgrade checklist | Checklist of missing needs for next level (default **on**) |
| **Ctrl+C** blueprint copy | Copy buildings under cursor, or active drag rectangle |
| **Ctrl+V** blueprint paste | Paste stamp (reports N/M and size) |
| **Ctrl+R** / **Ctrl+M** | Rotate / mirror blueprint (multi-tile safe) |
| Overlay house heatmaps | Needs / Food / Bath / Entertainment gap |

---

## Player-facing improvements

### Houses
- Classic evolve text still there.  
- Extra red line: e.g. `Des 8/16 · Water · Ent 10/25 · Pottery 0/1`.  
- Green “Ready to evolve” when nothing is missing.  
- Fixed bug: evolve message was sometimes garbage until you reopened the panel.

### Storage, industry & docks
- Free space on warehouses uses real empty slots.  
- Hints when understaffed or “emptying all”.  
- Farms/workshops show cart waiting reason or target.  
- **Docks**: status lines for ship state, import/export capacity, storage sinks, dock permission.

### Blueprint (multi-tile)
- Warehouse copies from compound top-left (3×3), not only the tower tile.  
- Rotate/mirror use full footprint size (warehouse, forts, gates, etc.).  
- Geometric rotation only for types that truly orient; color variants stay put.

### Logistics reliability
- Cart pushers re-find a destination if a warehouse stops accepting.  
- Partial unload: leftover loads go to another building.  
- Full workshops: re-route instead of looping.  
- Warehouse totals updated after every add/remove.  
- **Warehousemen**, **depot carts**, **suppliers**, **dockers** re-route on rejection / dead dests.  
- Mood uses **settled** tax/wage rates (blocks collection-tick cheese).

### Fairness & spam
- Undo after deleting houses restores the **same ages** (no free-worker exploit).  
- High display scale: mouse clicks match the cursor.  
- Low-sentiment messages only when **crossing** 48 / 40 / 35, with a longer delay.

### Build over walkers (config)
- Honored in ghost preview, wall placement, and burning-ruin rebuild checks.

---

## Optional rules (default off)

| Config | Effect |
|--------|--------|
| Storage accepts at 50%+ workers | Less strict than vanilla 100% staffing |
| Build over walkers | Placement not blocked only by figures on the tile |

---

## Docs

| File | Content |
|------|---------|
| [MILESTONE_A.md](MILESTONE_A.md) | Profiler, walker debug, first three fixes |
| [MILESTONE_BC.md](MILESTONE_BC.md) | House checklist, logistics UI, system fixes |
| [PERFORMANCE.md](PERFORMANCE.md) | Profiler labels, hotspots, player tips |
| [UPSTREAM_PR_PLAN.md](UPSTREAM_PR_PLAN.md) | Split this fork into small PRs for Keriew |

---

## Upstream PR tips

See [UPSTREAM_PR_PLAN.md](UPSTREAM_PR_PLAN.md). One concern per PR; config-gated rules default off; English `TR_*` for new strings.

No save-version change.
