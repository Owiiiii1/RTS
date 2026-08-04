# Cursor Work Report

## Task
GP-S27A1 — Resource Node Foundation

## Status
GP-S27A1_CODE_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s27a1-resource-node-foundation

## Base
main @ 74c058914a6349b3a9d0f83161023ff54b742be7

## ResourceNode architecture
- `AGP_ResourceNode : AActor` (replicated, always relevant, no tick, no movement replication)
- Not derived from `AGP_Unit` / `AGP_UnitBase` / Pawn / Character
- Root gameplay collision: `UBoxComponent` (~120×120×80 pile)
- Cosmetic: `UGP_ResourceNodeVisualComponent` (local build; not replicated parts)
- No GAS, combat, gather loop, team ownership, or command integration

## Exact native types
| Type | Role |
| --- | --- |
| `EGP_ResourceType` | `None`, `Ore` |
| `AGP_ResourceNode` | Server-authoritative deposit actor |
| `UGP_ResourceNodeVisualComponent` | Ore primitive visual |
| `EGP_PrimitiveShape` / `FGP_PrimitiveVisualPart` / `FGP_PrimitiveVisualDefinition` | Shared visual data (existing + Ore definition) |
| `GPPrimitiveVisualMesh` | Shared Engine BasicShapes resolve |
| `GPPrimitiveVisualBuilder` | Shared NoCollision part build/destroy |

## Collision policy
- Profile: **`BlockAll`** (documented; no new project channels)
- Root: `QueryAndPhysics`, `GenerateOverlapEvents=false`, `CanEverAffectNavigation=true`
- Visual parts: `NoCollision`, `CanEverAffectNavigation=false`
- Not treated as unit/pawn object type

## Replication policy
- `ResourceType`, `MaxAmount`: map-authored replicated configuration
- `CurrentAmount`: mutable replicated state via `OnRep_CurrentAmount`
- Initial + late join covered by actor replication + `bAlwaysRelevant`
- No NetMulticast

## ConsumeResource contract
- Authority only; client/no-authority → 0 (+ warning log)
- `RequestedAmount <= 0` → 0 (no-op)
- Returns actual consumed; clamps to `[0, MaxAmount]`
- Depletion does **not** destroy actor or change visuals
- No gather/workers/cooldown/events beyond state mutation + RepNotify

## Visual component strategy
**Option B:** thin `UGP_ResourceNodeVisualComponent` (no unit/team coupling).  
Shared mesh + builder helpers extracted so UnitVisual and ResourceNodeVisual do not duplicate 400+ lines. Full rename to a single `UGP_PrimitiveVisualComponent` deferred to avoid larger unit-path risk.

## Ore composition
5 parts: **Base** (Cylinder, PresentationRoot) + **Core** (Cone) + **AccentA/B/C** (Cone). No material, no animation, no idle tick. Dedicated suppresses parts.

## Dedicated behavior
- Actor + collision + replicated state exist
- Visual parts not constructed (`DedicatedVisualSuppressed`)
- No actor tick
- Listen server: single local composition (no duplicate rebuild path)

## Inspector command
`gp.ResourceNode.Inspect` (non-shipping): first valid node → Actor, ResourceType, Max/Current, Depleted, Role, NetMode, Replicates, AlwaysRelevant, collision fields, visual presence/built/parts/names, DedicatedVisualSuppressed, TickEnabled, VisualCollisionDisabled. Warning if none.

## Consume command
`gp.ResourceNode.Consume <Amount>` (non-shipping): authority worlds only; client prints refusal; logs requested/consumed/before/after.

## Navigation policy
Root `CanEverAffectNavigation=true` for Recast when a map exists. Visual parts never nav-relevant. No map/navmesh authored in S27A1. Note: current unit Move uses non-sweep teleport; physical “walk through” vs nav block should be validated when arena nav lands.

## Files changed
- `GP/Source/GPRuntime/Public/Resources/GPResourceTypes.h` (new)
- `GP/Source/GPRuntime/Public/Resources/GPResourceNode.h` (new)
- `GP/Source/GPRuntime/Private/Resources/GPResourceNode.cpp` (new)
- `GP/Source/GPRuntime/Public/Visual/GPResourceNodeVisualComponent.h` (new)
- `GP/Source/GPRuntime/Private/Visual/GPResourceNodeVisualComponent.cpp` (new)
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualMesh.h` (new)
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualMesh.cpp` (new)
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualBuilder.h` (new)
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualBuilder.cpp` (new)
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualTypes.h`
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualTypes.cpp`
- `GP/Source/GPRuntime/Public/Visual/GPUnitVisualComponent.h`
- `GP/Source/GPRuntime/Private/Visual/GPUnitVisualComponent.cpp`
- `Docs/Development/Claude_Tasks/GP-S27A1_Resource_Node_Foundation.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development (+ UHT via new headers) | **PASSED** |
| GP Win64 Development | not run |
| GP Win64 Shipping | not run |

## Operator validation steps

### A. Place node
1. Open Editor on any convenient map (temporary; do not commit `.umap`).
2. Place `AGP_ResourceNode` in the world; save locally if needed for PIE.

### B. Listen server + remote client
1. PIE listen host + one client.
2. Confirm node visible both sides; same composition silhouette.
3. Confirm root collision present; visual parts have NoCollision.
4. Move units around the node; node must not enter selected-units as a unit.
5. Expect: unit cannot path “through” once nav exists; with current non-sweep Move, verify collision component settings via Inspect if physical block is inconclusive.

### C. Inspector
1. Host: `gp.ResourceNode.Inspect`
2. Client: `gp.ResourceNode.Inspect`
3. Confirm fields match roles/netmode; VisualBuilt true on listen/client; DedicatedVisualSuppressed false there.

### D. Consume
1. Host: `gp.ResourceNode.Consume 100` → amount decreases; client sees replicated CurrentAmount.
2. Client: same command → refused.
3. Host: `gp.ResourceNode.Consume 0` / negative → no-op.
4. Host: consume until 0 → actor remains; visuals unchanged; `Depleted=true`.

### E. Dedicated
- Confirm by code + Inspect policy (`DedicatedVisualSuppressed` when NM_DedicatedServer).
- Operator dedicated run: **not performed** for this candidate (optional).

## Known limitations
- Harvest still expects `AGP_UnitBase` + `GP.Resource.Node` — not wired to `AGP_ResourceNode`
- No selection UI / Move / Attack for nodes
- No map, navmesh, gather, economy, depletion cosmetics
- Engine BasicShapes may look uniform gray (acceptable for S27A1)

## Commit SHA
e2bf9ea86c7e49e1a4b841577d9b737fbf05a778

## Git State
- Feature branch only; no main change; no PR; no merge; no `.umap`; no editor module; S27A2 not started
