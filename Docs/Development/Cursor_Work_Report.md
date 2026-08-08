# Cursor Work Report — GP-S32R Orbital Building Drop

## Status
**GP-S32R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

---

## 1. Slice ID decision
**Chosen:** `GP-S32R_Orbital_Building_Drop`

**Why:** Historical docs reserve **GP-S34** for BuildingBase + BuildingDefinition foundation (`AGP_BuildingBase` already exists “ahead of full GP-S34”). No canonical ID reserved specifically for *orbital building drop*. Per operator fallback when audit is ambiguous → **GP-S32R**.

Branch: `feature/gp-s32r-orbital-building-drop`

## 2. Base / HEAD
| | SHA |
|---|---|
| Base (`origin/main`) | `427a2aa6d9bc4c3733a044ec9b09d053d10dcc59` |
| Implementation | `c44a80f` |
| Work-report note | `e36131f` |
| HEAD after overlap/contract fix | *(this commit)* |

## 3. Pre-implementation audit (factual)
| Item | Result |
|---|---|
| `UGP_BuildingDefinition` | **Absent** in Source |
| `AGP_BuildingBase` | Present (abstract) |
| `AGP_LogisticsHub` | Did not exist → added minimal native identity |
| BuildGrid / FoW | **Not production-ready** → INTERIM_MVP_PLACEMENT_VALIDATION |
| Shared DropPod | `AGP_DropPod` from GP-S31R reused |

## 4. Catalog decision
**Path B:** `UGP_OrbitalDeliverySettings` building catalog keys (cost, soft `BuildingPayloadClass`, timings, radius/margin).  
**Why:** Full `UGP_BuildingDefinition` DataAsset expands scope; settings path matches unit-drop precedent and is enough for one MVP type.

## 5. Architecture
| Area | Decision |
|---|---|
| READY inventory | `UGP_OrbitalBuildingInventoryComponent` on `AGP_PlayerState`, OwnerOnly |
| Purchase | `RequestBuildingPurchase` → GAS `UGP_GE_SpendOrbital` once → Ready++ |
| Deploy | validate READY + placement → spawn pod → consume READY (destroy pod if consume fails) — **no Orbital spend** |
| Ghost | local `AGP_BuildingPlacementGhost` (translucent box; no gameplay BeginPlay) |
| Placement | INTERIM: finite transform, radius from MainBase, **deterministic capsule-extent check vs `AGP_BuildingBase`** (not ECC_Pawn overlap — building capsules Ignore Pawn) |
| DropPod | `EGP_DropPodPayloadKind::Building` + `AuthorityInitBuildingDrop` |
| Ground | `GPBuildingGroundPlacement` capsule half-height |
| Timing | Independent BuildingDrop* seconds in settings |
| TEMP HUD | BUILDINGS panel: Cost / READY / Purchase / Deploy READY |
| MVP building | Native `AGP_LogisticsHub` + soft BP seam; **no MaxUnits bonus** |

## 6. Exact-once
- Purchase: spend then Ready++; invalid requests reject with no mutation
- Deploy: Ready checked before spawn; consume after successful pod spawn
- Cancel placement: Ready unchanged (client ghost only)
- Payload spawn: DropPod `bPayloadSpawned` guard

## 7. Build
GPEditor Win64 Development + UHT: **PASS**

GP Development / Shipping: **NOT RUN** (candidate)

## 8. Contracts (NullRHI `-game`)
| Command | Result |
|---|---|
| `gp.Building.RunOrbitalBuildingDropContractTest` | **Failures=0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **Failures=0** |
| `gp.Resource.RunS28RegressionSuite` | **Failures=0** |
| `gp.Resource.RunContainerLaunchContractTest` | **Failures=0** |
| `gp.Resource.RunContainerLaunchHUDContractTest` | **Failures=0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **Failures=0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **Failures=0** |
| `gp.Combat.RunLOSFireGateContractTest` | **Failures=0** |

## 9. Candidate fix after first NullRHI run
Building capsules use Visibility-only collision (`ECC_Pawn` Ignore). Physics `OverlapMultiByChannel(ECC_Pawn)` never saw MainBase → false-accept at base location. Replaced with deterministic building-iterator + capsule radii (**INTERIM_MVP_PLACEMENT_VALIDATION**). Contract hardened for null pod + clears prior hubs before stub deploy.

## 10. Operator assets untouched
DefaultEngine.ini, operator BP soft-class paths, map, Blueprint/, Materials/, Niagara/VFX packs, Tools/, local `.uasset`/`.umap` — **not committed**.

`DefaultGame.ini`: only appended `Building*` orbital settings keys.

## 11. Deferred
- BuildGrid / FoW (canonical placement)
- Logistics Hub +5 MaxUnits / container bonus
- Defensive Turret combat
- Walls
- Production Order Menu polish
- Full `UGP_BuildingDefinition` DataAsset (still reserved historically under GP-S34 foundation)

## 12. Operator PIE target
1. Earn Orbital → Purchase Logistics Hub → READY=1  
2. Deploy READY → ghost → RMB/Esc cancel → READY still 1  
3. Deploy → LMB valid → READY=0, Orbital unchanged  
4. Shared BP_DropPod_MVP descends → building Niagara phase → hub on ground with TeamId  
5. Unit drop still works
