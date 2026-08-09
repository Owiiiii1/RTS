# Cursor Work Report — GP-S32R Input Ownership Fix

## Status
**GP-S32R_INPUT_OWNERSHIP_FIX_READY_FOR_OPERATOR_RETEST**

NOT MERGED.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s32r-orbital-building-drop` |
| Prior candidate | `b922b18a6a75066b6d85d380d6e0e1b19c61771b` |
| Final commit | 71c19854f45fd659e35cc9b8ab4cc609e9799674 |

## 2. Operator bug
While a unit was selected, **Deploy READY** created the placement ghost and **RMB cancelled the ghost**, but the **same RMB also issued a normal RTS command** to the selected unit.

Esc is not required for acceptance (PIE window capture).

## 3. Root cause
`Tick` cancelled placement on RMB edge (`CancelBuildingPlacement`), clearing `bBuildingPlacementActive`, while **`OnCommandInputStarted` still ran for that RMB** and built/sent `Server_RequestCommand` because there was **no placement ownership gate on the command path**.

Selection was also left intact on enter, so units remained command targets.

## 4. Selection clear on enter
`EnterBuildingPlacementMode` → `ClearSelectionForBuildingPlacementEnter()`:
- `CancelActiveMarquee`
- `SelectionComponent->ClearSelection()`
- `ClearInspectedTarget()`
- `CancelMarquee()`
- clears local press state

Control groups are **not** erased.

## 5. Command suppression
`OnCommandInputStarted` calls `ConsumeBuildingPlacementCommandInput()` first:
- if placement active → `CancelBuildingPlacementFromRMB()` + return (no command RPC)
- if `bBuildingPlacementSuppressCommandUntilRMBRelease` → return

Tick / `UpdateBuildingPlacementInputEdgesForContract` also cancel via RMB edge and set the same suppress-until-RMB-release flag (prevents Tick-vs-input race click-through).

## 6. Selection suppression
While placement active:
- `OnSelectionStarted` tracks LMB for **confirm only** (no marquee / click-select)
- `OnSelectionCompleted` → `ConfirmBuildingPlacement` only
- `Tick` returns before `UpdatePendingSelectionDrag`

`IsBuildingPlacementSelectionInputBlocked()` also true while confirm-suppress-until-LMB-release is set.

## 7. Click-through prevention
| Transition | Guard |
|---|---|
| HUD Deploy (LMB held) | `bBuildingPlacementSuppressConfirmUntilLMBRelease` seeded from LMB-down on enter |
| RMB cancel | suppress command until RMB release |
| LMB confirm | suppress selection until LMB release after confirm |

## 8. Semantics
| Mode | LMB | RMB |
|---|---|---|
| Placement | Confirm | Cancel only (READY unchanged) |
| Normal RTS | Selection / marquee | Contextual command |

Esc → still calls `CancelBuildingPlacement` via selection cancel if bound; not required for operator pass.

## 9. Build
GPEditor Win64 Development + UHT: **PASS**  
GP Dev / Shipping: **NOT RUN**

## 10. Contracts (NullRHI `-game`) — Failures=0
| Command | Result |
|---|---|
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** (includes P_* input ownership seams) |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunContainerLaunchHUDContractTest` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |

## 11. Files changed
- `GPPlayerController.h` / `.cpp` — selection clear, command/selection gates, suppress-until-release, contract seams
- `GPOrbitalBuildingDropContractTest.cpp` — P_* input ownership assertions
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/AI_Project_Log.md` (brief)

## 12. Operator assets untouched
DefaultEngine.ini, operator DefaultGame.ini soft paths, map, Blueprint/, Materials/, Niagara/VFX, Tools/, `.uasset`/`.umap` — **not committed**.
