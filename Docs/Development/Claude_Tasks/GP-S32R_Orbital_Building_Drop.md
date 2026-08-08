# GP-S32R — Orbital Building Drop

## Status
**GP-S32R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s32r-orbital-building-drop`  
Base: GP-S31R finalization tip @ `427a2aa`

## Scope
- `UGP_OrbitalDeliverySettings` building catalog keys (cost, payload soft ref, drop timing, deploy radius)
- Native `AGP_LogisticsHub` (capsule root, no MaxUnits bonus)
- `UGP_OrbitalBuildingInventoryComponent` on `AGP_PlayerState` (OwnerOnly READY)
- `GPBuildingDropAuthority` — Purchase (spend once → READY++) / Deploy (validate → pod → consume READY)
- `AGP_DropPod` building payload kind + `AuthorityInitBuildingDrop`
- `GPBuildingGroundPlacement` capsule offset helper
- Local `AGP_BuildingPlacementGhost` + PC placement mode (LMB confirm / RMB cancel)
- TEMP HUD BUILDINGS panel (Purchase / Deploy READY)
- Contract: `gp.Building.RunOrbitalBuildingDropContractTest` (A–O)

## Architecture
- **Purchase:** validate catalog + MainBase + Orbital → `UGP_GE_SpendOrbital` once → `ReadyLogisticsHubCount++`
- **Deploy:** validate READY + interim placement → spawn DropPod → consume READY (rollback if spawn fails)
- **No Orbital spend on deploy**
- Building pods reuse `UnitDropPodClass` / `ResolveUnitDropPodClass()`
- Interim placement (`INTERIM_MVP_PLACEMENT_VALIDATION`): finite transform, distance from own MainBase, **deterministic capsule-extent overlap vs `AGP_BuildingBase`** (not ECC_Pawn physics — building capsules Ignore Pawn / Visibility-only)

## Contracts (verified NullRHI `-game`)
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

## Builds
| Target | Policy |
|---|---|
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development / Shipping | **NOT RUN** (candidate slice) |

## Operator assets untouched
DefaultEngine.ini, operator BP soft paths (Worker/SW/DropPod), map, Blueprint/, Materials/, Niagara packs, Tools/, local .uasset/.umap.

## Stop Condition
Operator validation in PIE: Purchase → READY++, Deploy ghost → pod lands → Logistics Hub controllable. Do **not** auto-merge.
