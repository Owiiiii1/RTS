# GP-S27A1 Resource Node Foundation

## Status
**GP-S27A1_FINALIZED_READY_FOR_MERGE**

Overall: **GP-S27A1_DONE_RESOURCE_NODE_FOUNDATION**

## Baseline
`main` @ `74c058914a6349b3a9d0f83161023ff54b742be7`

Architecture source: `Docs/Development/Claude_Tasks/GP-S27A_Prototype_Arena_Analysis.md`  
Branch: `feature/gp-s27a1-resource-node-foundation`  
Implementation: `e2bf9ea86c7e49e1a4b841577d9b737fbf05a778`  
Ore visual correction: `51a310acfd2200d2bedd0b2a03221274d09f73d0`

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
- No team ownership

### Resource state
| Property | Defaults | Notes |
| --- | --- | --- |
| `ResourceType` | Ore | EditAnywhere + replicated (map-authored config) |
| `MaxAmount` | 5000 | EditAnywhere + replicated |
| `CurrentAmount` | 5000 | EditInstanceOnly + RepNotify; clamp 0..Max |

API: `GetResourceType`, `GetMaxAmount`, `GetCurrentAmount`, `IsDepleted`, authority-only `ConsumeResource(int32)`.

### ConsumeResource contract
- Authority only; `RequestedAmount <= 0` → 0
- Returns actual consumed; never below 0; over-request clamps to remainder
- No workers/gather/cooldown; no destroy/hide on deplete; no multicast

### Collision / navigation
- Profile: **`BlockAll`** (explicit; no new global channels)
- Root: `QueryAndPhysics`, `GenerateOverlapEvents=false`, `CanEverAffectNavigation=true`
- Visual parts: `NoCollision`, no nav relevance
- Not ECC_Pawn / not unit-shaped

### Visual strategy (B)
Thin `UGP_ResourceNodeVisualComponent` + shared helpers:
- `GPPrimitiveVisualMesh` (Engine BasicShapes resolve)
- `GPPrimitiveVisualBuilder` (NoCollision build/destroy)
- Reuses `EGP_PrimitiveShape`, `FGP_PrimitiveVisualPart`, `FGP_PrimitiveVisualDefinition`
- No team tint; dedicated suppresses render parts
- `UGP_UnitVisualComponent` uses the same builder (dedupe)

### Ore composition (5 parts)
Base (Cylinder, PresentationRoot) + Core (Cone) + AccentA/B/C (Cone).  
Visual correction: uniform sunk Base; tall Core; elongated outward-leaning accents (Cone +Z tip up).

### Console (non-shipping)
- `gp.ResourceNode.Inspect`
- `gp.ResourceNode.Consume <Amount>` (authority world only)

## Intentionally not done
Map, editor module, generator, Blueprint, DataAsset, meshes/materials import, gather/harvest/economy, command-system changes, selection-as-unit, team tint, S27A2+.

## Operator validation (accepted)

| Area | Result |
| --- | --- |
| Functional (visibility, consume, clamp, deplete, selection, collision/nav/visual inspect, no tick) | **PASS** |
| Network (Authority / SimulatedProxy, replicated amounts, AlwaysRelevant, no duplicate visuals) | **PASS** |
| Visual readability (post-correction crystal-pile silhouette) | **PASS** |

## Known limitations
- Harvest command still expects `AGP_UnitBase` + resource tag — **not integrated**
- ResourceNode is not in selected-units array / Move / Attack
- Current `UGP_MovementComponent` uses non-sweeping `SetActorLocation`; physical Move block vs nav is a later arena concern
- Dedicated operator run not executed (suppression confirmed by code / Inspect policy)
- No depletion visual states; Engine BasicShapes may look uniform gray

## Build

| Target | Result | Notes |
| --- | --- | --- |
| GPEditor Win64 Development | **PASSED** | At visual correction `51a310acfd2200d2bedd0b2a03221274d09f73d0` (not re-run; C++ frozen at finalization) |
| GP Win64 Development | **PASSED** | Finalization |
| GP Win64 Shipping | **PASSED** | Finalization |

No known blockers. Ready for main merge when requested.
