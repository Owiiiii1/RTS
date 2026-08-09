# Cursor Work Report — GP-S33M RTS Movement Reconciliation (operator defect revision)

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s33m-rts-movement-reconciliation` |
| Base (`main`) | `0df4468445e939aaca33ed73548a78c2caabb86d` |
| Prior candidate | `f72794da169dd8efd3b881443453655547b63264` |
| This revision | `1e34ae348fe99e1cac74b36e26254407128486cf` (SHA note `796e6c4eddc31d73eba13250baad3ef15a90e42e`) |

## 2. Operator first-pass facts
| Check | Result |
|---|---|
| NavMesh on L_PrototypeArena | **PASS** |
| Units use navigation | **PASS** |
| Unit↔unit avoidance | **PASS** |
| Building nav obstacle | **FAIL** (no authored footprint) |
| Reassigned Worker CargoFull→Haul | **FAIL** (stood with full cargo) |

## 3. Worker bug — root cause
`HandleMiningStateChanged` treated `EGP_MiningState::Idle` as a mine-chain terminal.

During reassignment / remine, `BeginMining` can call `StopMining(ManualStop)` → Idle **while UnitCommand is still bound** inside `BeginMiningAtHeldTarget`.

That Idle handler:
1. cleared `ActiveMineSerial`
2. cleared Held Mine
3. unbound `OnMiningStateChanged`

Then `BeginMining` continued and reached `Started` / mining on deposit B **without** UnitCommand listening.

Later `CargoFull` broadcast had **no** UnitCommand consumer → no `StartHaulReturnToBase`.

Manual Mine while full correctly hit `MineRejected: CargoFull` (intentional; not the bug).

## 4. Worker fix
- Ignore Idle terminals **only** when `bBeginMiningAtHeldTargetInProgress` (internal remine).
- External Idle (player/test `StopMining`) still clears the chain.
- Belt-and-suspenders: stop prior-node occupancy **unbound** before `Bind`+`BeginMining`; arrival/`RejectedCargoFull` starts haul instead of clearing Held.

### State ownership after reassignment
| Field | Role |
|---|---|
| `HeldCommand.TargetActor` | Active Mine intent deposit (updated by `TryRetargetMineToNode` to B) |
| `MineTarget` | Runtime active deposit (B after retarget/approach) |
| `ActiveMineSerial` | Shared Mine/Haul chain identity |
| Manual `Mine` + full cargo | Still rejected `CargoFull` unless `IsActiveHaulChainForDeposit` |

## 5. Building NavigationObstacle
| Item | Value |
|---|---|
| Component | `UBoxComponent* NavigationObstacle` on `AGP_BuildingBase` |
| BP seam | Inherited `GP\|Navigation`; Relative Location / Rotation / Box Extent editable |
| Collision | QueryOnly; **all channels Ignore** (no Visibility / Pawn gameplay block) |
| Nav | `CanEverAffectNavigation=true`, `bDynamicObstacle=true`, `SetAreaClassOverride(UNavArea_Null)` |
| Defaults | MainBase extent `(160,160,130)`; LogisticsHub `(140,140,120)`; base default `(140,140,120)` |
| Capsule | Remains non-nav (`SetCanEverAffectNavigation(false)`) |

### Runtime Recast
Orbital/runtime-spawned buildings need Recast **Runtime Generation = Dynamic** (or Dynamic Modifiers Only) for dynamic obstacles to update nav at runtime.

**Do not** edit operator `DefaultEngine.ini` / map in this slice.

Operator must set Project Settings → Navigation Mesh → Runtime Generation locally if runtime Hub spawn must carve nav in PIE.

Static placed buildings with baked nav / editor rebuild still benefit from the authored footprint when paths are built.

## 6. Contracts / regressions
| Command | Failures |
|---|---|
| `gp.Movement.RunRTSMovementReconciliationContractTest` | **0** (incl. Building_* seam checks) |
| `gp.Resource.RunMineReassignmentHaulContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunContainerLaunchHUDContractTest` | **0** |
| `gp.Combat.RunAttackMoveContractTest` | **0** |
| `gp.Combat.RunAutoAcquireContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** |

## 7. Builds
- GPEditor Win64 Development + UHT **PASS**
- GP Dev / Shipping **not run**

## 8. Config / map / content
**None committed.** Operator-local ini/umap/assets untouched.

## 9. Exact operator retests

### TEST A — Building
1. Open BP child of MainBase or LogisticsHub.
2. Confirm inherited **NavigationObstacle** in Components.
3. Move / rotate / resize Box Extent.
4. PIE: RMB Move unit to opposite side of building → path goes around authored footprint.
5. If testing **runtime** orbital Hub: ensure Navigation Mesh Runtime Generation = **Dynamic** (local setting).

### TEST B — Worker
1. Two deposits; fill deposit A so no free mining place.
2. Mine A → Worker reassigns to B → mines to full → **automatically** hauls to MainBase → unloads → resumes B.
3. No manual Mine after CargoFull.
4. Manual Mine while already full still rejects `CargoFull`.

## 10. Known limitations
- Headless contract does not prove Recast carve around building (operator PIE).
- Dynamic orbital building nav requires Runtime Generation=Dynamic (operator-local).
