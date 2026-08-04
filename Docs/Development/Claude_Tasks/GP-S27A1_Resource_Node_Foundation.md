# GP-S27A1 Resource Node Foundation

## Status
**GP-S27A1_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `74c058914a6349b3a9d0f83161023ff54b742be7`

Architecture source: `Docs/Development/Claude_Tasks/GP-S27A_Prototype_Arena_Analysis.md`  
Branch: `feature/gp-s27a1-resource-node-foundation`  
Implementation: `e2bf9ea86c7e49e1a4b841577d9b737fbf05a778`

## Goal
Server-authoritative Ore `AGP_ResourceNode` suitable for later persistent placement on `L_PrototypeArena`. No map, editor module, gather loop, Blueprint, or DataAsset in this slice.

## Shipped

### Resource types
- Native `EGP_ResourceType`: `None`, `Ore`

### Actor
- `AGP_ResourceNode : AActor` (not unit/pawn/character)
- Replicates; `bAlwaysRelevant = true`; no permanent tick; no movement; no combat; no GAS
- Root `UBoxComponent` gameplay collision (~120×120×80 pile volume)
- `UGP_ResourceNodeVisualComponent` for Ore primitive composition

### Resource state
| Property | Defaults | Notes |
| --- | --- | --- |
| `ResourceType` | Ore | EditAnywhere + replicated (map-authored config) |
| `MaxAmount` | 5000 | EditAnywhere + replicated |
| `CurrentAmount` | 5000 | EditInstanceOnly + RepNotify; clamp 0..Max |

API: `GetResourceType`, `GetMaxAmount`, `GetCurrentAmount`, `IsDepleted`, authority-only `ConsumeResource(int32)`.

### ConsumeResource contract
- Authority only; `RequestedAmount <= 0` → 0
- Returns actual consumed; never below 0
- No workers/gather/cooldown; no destroy/hide on deplete; no multicast

### Collision policy
- Profile: **`BlockAll`** (explicit; no new global channels)
- Root: `QueryAndPhysics`, `GenerateOverlapEvents=false`, `CanEverAffectNavigation=true`
- Visual parts: `NoCollision`, no nav relevance
- Not ECC_Pawn / not unit-shaped

### Visual strategy (B)
Thin `UGP_ResourceNodeVisualComponent` + shared helpers extracted from unit path:
- `GPPrimitiveVisualMesh` (Engine BasicShapes resolve)
- `GPPrimitiveVisualBuilder` (NoCollision build/destroy)
- Reuses `EGP_PrimitiveShape`, `FGP_PrimitiveVisualPart`, `FGP_PrimitiveVisualDefinition`
- No team tint; no `UGP_UnitVisualComponent` reuse on the node
- `UGP_UnitVisualComponent` refactored to call the same builder (dedupe)

### Ore composition (5 parts)
Base (Cylinder) + Core (Cone) + AccentA/B/C (Cone). Dedicated: no render parts.

### Ore visual readability correction
Operator functional matrix **PASS**; prior Ore read as flat disk + blobs. Native transforms retuned only (`MakeOreNodeDefinition`):

| Part | New Loc / Rot / Scale |
| --- | --- |
| Base | (0,0,-40) / I / (0.56,0.56,0.56) — sunk uniform pedestal |
| Core | (0,0,98) / I / (0.52,0.52,3.85) — tall central crystal |
| AccentA | (52,8,82) / Pitch 28 / (0.36,0.36,2.75) — lean +X |
| AccentB | (-10,-54,76) / Roll -30 / (0.32,0.32,2.45) — lean -Y |
| AccentC | (-46,40,68) / (-18,12,24) / (0.28,0.28,1.95) — lean +Y/-X |

Root cause of flat look: old Base scale Z=0.28 squashed child Cones. Visual recheck pending.

### Console (non-shipping)
- `gp.ResourceNode.Inspect`
- `gp.ResourceNode.Consume <Amount>` (authority world only)

## Intentionally not done
Map, editor module, generator, Blueprint, DataAsset, meshes/materials import, gather/harvest, command-system changes, selection-as-unit, team tint, S27A2+.

## Known limitations
- Harvest command still expects `AGP_UnitBase` + resource tag — **not integrated**
- ResourceNode is not in selected-units array / Move / Attack
- Current `UGP_MovementComponent` uses non-sweeping `SetActorLocation`; physical blocking for unit Move is nav/future concern — root is still nav-blocking configured for Recast once a map exists
- Dedicated operator run not required for this candidate
- No depletion visual states
- Ore visual readability awaiting operator recheck after transform correction

## Build
- GPEditor Win64 Development + UHT — **PASSED** (implementation)
- GPEditor Win64 Development — **PASSED** (Ore visual correction)
- GP Development / Shipping — not run (deferred to finalization)

## Operator validation
Functional matrix PASS (see Cursor report). Visual readability recheck pending after correction.
