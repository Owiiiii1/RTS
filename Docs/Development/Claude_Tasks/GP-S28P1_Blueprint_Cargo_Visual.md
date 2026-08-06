# GP-S28P1 — Blueprint-Ready Resource Actors + Cargo Visual Contract

## Status
**GP-S28P1_READY_FOR_MERGE**

## Baseline
- Branch: `feature/gp-s28p1-blueprint-cargo-visual`
- Base audit: `audit/gp-s28p-resource-playable-pass` @ `377b9b8c28dc09929efbae061a05e351b0dbad3f`
- Main: `035c486758059032bb2551520834dd73f8667ef5` (**untouched**)

## Commit inventory
| SHA | Role |
| --- | --- |
| `e196a43e124e4c9fb0b0fe7f56ae299ac61f459a` | Presentation roots + cargo visual |
| `eea992a312af2a73400ad4f6d0bece2e82d73bf5` | Worker UnitDefinition warning removal |
| `70c8578aa70595f104732548862dc2f554b627c0` | Storage validation lifecycle + MainBase BuildingDefinition warning |
| `ca40d1bbc2087954dff11be9e9f3fe87eabe6aed` | MiningEffectAnchor + Niagara effect event + generated visual override |

## Operator validation — PASSED
- BP Worker/MainBase compile clean (no UnitDefinition / BuildingDefinition / Containers-size blockers)
- Full Mine → haul → unload → remine loop
- Cargo color presentation (always-visible container; FillNormalized white/yellow/green)
- Niagara mining effect only while Mining
- ResourceNode `Use Generated Prototype Visual = false` keeps authored meshes + CollisionBox / RMB Mine

## Automated tests
Non-interactive PIE console automation **not** claimed as Failures=0 in this environment (OpenWorld `-game` ExecCmds previously hung). Operator PIE validation **PASSED**. Tests not weakened.

## Final builds — PASSED
- GPEditor Win64 Development + UHT
- GP Win64 Development
- GP Win64 Shipping

## Delivered

### Worker
- `PresentationRoot` / `CargoVisualAnchor` / `MiningEffectAnchor`
- `OnCargoVisualStateChanged` (API stable; operator: always-visible mesh; color via FillNormalized)
- `OnMiningEffectStateChanged` — active only for `EGP_MiningState::Mining`
- Cargo/Mining components remain SoT; no presentation-only replicated bool; no permanent Tick

### MainBase
- `PresentationRoot` / `DropOffVisualAnchor`
- Lifecycle-aware Storage validation; authority BeginPlay `EnsureContainerArray`; **5×100** defaults

### ResourceNode
- `bUseGeneratedPrototypeVisual` default **true**
- false clears generated prototype parts; authored + CollisionBox preserved

## Preserved
Mine/haul/FIFO/depletion/Storage LOST/Threat/registry/combat/HUD unchanged. No BP/Niagara/material/map committed (operator local assets stay local).

## Out of scope (P2+)
Depletion lifecycle presentation, cross-node reassignment, drop-off wait, HUD, launch/Orbital/Score, combat.

## Stop / merge
READY_FOR_MERGE on feature branch (finalization `ae1450d713c59231116600ac1548f956d9f5aed7`). Do **not** open PR / merge / touch main until operator/tech-lead assigns.
