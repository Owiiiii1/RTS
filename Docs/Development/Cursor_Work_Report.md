# Cursor Work Report

## Task
GP-S27A Persistent Prototype Arena + One-Shot Editor Generator + Ore Node — analysis

## Status
GP-S27A_ANALYSIS_READY_FOR_REVIEW

## Branch
feature/gp-s27a-prototype-arena-analysis

## Base
main @ 805756d12477b23bf7794f674e534fafba8ce360

## Current Map / Editor State
- No project `.umap`; Content = Enhanced Input only
- `GameDefaultMap` = Engine OpenWorld template
- GameMode `AGP_GameMode` / CameraPawn / PC configured
- **No Editor module** (only `GPEditor` Target loading Runtime modules)
- No NavMesh/Recast usage in project C++; no ResourceNode actor
- Units: placeable `AGP_Unit`; no tracked level population
- Git LFS already tracks `*.umap` / `*.uasset`
- Python editor plugin: not enabled

## Chosen Map Strategy
**A — new compact non–World-Partition** `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`  
Rejected: WP arena, Engine OpenWorld ownership, Data Layer complexity for MVP.

## Chosen Editor Generator Strategy
**Primary:** Editor-module console command + menu (`gp.Editor.GeneratePrototypeArena`) via subsystem.  
**Fallback:** `CallInEditor` actor calling same code.  
Never PIE; abort if map exists; explicit rebuild command separate.

## Editor Module Decision
Add **`GPEditor`** Editor-type module (Build.cs, IMPLEMENT_MODULE, generator, menus); register in `.uproject` + `GPEditor.Target.cs`. Not in Game target → absent from packaged game.

## Resource Node Architecture
`AGP_ResourceNode : AActor` (replicated): Ore type, Max/Current amount; root gameplay collision; primitive visuals via shared/thin visual component — **not** a unit subclass. Gather/deplete out of scope. Selection optional/deferred if still unit-only.

## Ore Visual Proposal
Base Cylinder + Core Cone + 3–4 accent Cones (≤6 parts); visual NoCollision; no material/anim; RTS-readable crystal pile.

## Exact Arena Layout Proposal
4000×4000 floor; Team1 (−1400) / Team2 (+1400); melee/blocked/kill/OOR test pairs (OOR ~450 given range 250); 5 ore; 6–8 obstacles; lights; PlayerStart; NavMeshBounds; ~40–50 actors; tag `GP.GeneratedPrototypeArena`; deterministic labels.

## Navigation Strategy
Introduce Recast + bounds; editor build after generate; save with map; ore/obstacles block nav; no runtime regen for MVP.

## Multiplayer Implications
Persistent map only; server-authoritative units/ore; no runtime duplicate generation; editor code not in game build.

## Idempotency / Rebuild Policy
Default **abort if exists**; explicit rebuild command; generated-actor tag; companion layout manifest.

## Binary Map / Git Policy
LFS `.umap` (already configured); non-reviewable binary diff; companion `.layout.md`/`.json` with version/seed/actor table; deterministic generator.

## Implementation Slices
- **S27A1** ResourceNode foundation (no map)
- **S27A2** Editor module + empty arena save
- **S27A3** Populate units/ore/tests + nav + validation

## Validation Matrix
See analysis §14 (node / generator / arena).

## Rejected Options
Runtime/PIE generation; WP-first; OpenWorld as owned map; Ore-as-Unit; Python-first; silent overwrite.

## API Gaps Noted
Per-instance combat stats need EditInstanceOnly overrides or editor helpers (Defaults are EditDefaultsOnly today). Harvest command currently expects `AGP_UnitBase` + resource tag — align when gather lands.

## Files Changed
- `Docs/Development/Claude_Tasks/GP-S27A_Prototype_Arena_Analysis.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Diff Status
- C++ diff: **none**
- Assets / umap diff: **none**
- Build: **not required**

## Commit SHA
COMMIT_SHA_PLACEHOLDER

## Git State
- Push to `feature/gp-s27a-prototype-arena-analysis`
- No merge to main; no PR; no implementation; no `.umap` created
