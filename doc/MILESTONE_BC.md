# Milestone B & C — Feel, logistics, quality

Built on Milestone A. Safe for saves (no format changes). Gameplay toggles default **off** except the house checklist (on).

## B — Feels modern

### House upgrade checklist
- **Config:** UI → *Show house upgrade checklist* (`ui_house_diagnose`, default on)
- Click a house: under the classic evolve sentence you get a short list of **all** missing needs for the next level (desirability, water, entertainment, food types, pottery/oil/furniture/wine, space…).
- Same rules as the real evolution code — not a second fake system.

### Economy transparency
- **Warehouse / granary:** free space uses recounted empty slots; orange hint if understaffed or emptying.
- **Farms / industry:** one line for cart status (waiting / understaffed / destination).

## C — Systems / correctness

| Fix | Detail |
|-----|--------|
| Partial cart delivery | Remaining loads re-route to another sink |
| Workshop full | Mid-trip revalidation; no forced unload into full workshop |
| Warehouse recount | Main counters stay correct after add/remove |
| evolve_text_id clobber | Tick path no longer overwrites UI text ids with -1/0/1 |

### Optional gameplay (default off)
| Option | Effect |
|--------|--------|
| Storage accepts at 50%+ workers | Less “invisible” logistics choke |
| Build over walkers | Placement not blocked only by figures |

## PR notes for upstream

- Prefer reviewing as **separate topics**: house diagnose UI · cart logistics · warehouse counters · config options.
- No save-version bumps.
- English strings for new config keys; checklist labels use short English codes (Des/Ent/…) for compactness.
- Manual test: large house missing bath+food; farm with full warehouse + empty workshop; delete house + undo ages; display scale 200% clicks.

## Not in this milestone

- Full blueprint / copy-paste city tools  
- Map heatmaps for every service  
- Full pathfinding rewrite  
