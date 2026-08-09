# Cursor Work Report — GP-S33M RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s33m-rts-movement-reconciliation` |
| Base (`main`) | `0df4468445e939aaca33ed73548a78c2caabb86d` |
| Candidate | *(recorded after commit — see HEAD)* |

## 2. Old factual movement model
- `UGP_MovementComponent` server-authoritative straight-line tick
- `SetActorLocation(..., false)` (no sweep)
- Capsule QueryOnly; Pawn ignored
- Results: `Reached` / `Cancelled` only
- Multi-select Move/AttackMove: identical `TargetLocation` for all units

## 3. New pathfinding implementation
- `RequestMove` → project start/end on NavMesh → `FindPathSync` → waypoint follow
- Rate-limited mid-path repath (`RepathIntervalSeconds`, default 0.75s)
- No NavData **or** unit outside nav coverage → straight 2-point fallback
- On-nav unreachable → sync reject (`PathNotFound` / `DestinationOffNav`) or terminal `Failed`
- Short/medium on-nav legs: trivial/soft straight between projected points when Recast sync fails (mine corrective safety)

## 4. Navmesh requirements
- `GPRuntime` depends on `NavigationSystem` (already present)
- Operator must ensure `NavMeshBoundsVolume` on playable maps for production pathfinding
- **Do not** commit `L_PrototypeArena.umap` / DefaultEngine.ini / DefaultGame.ini

## 5. Movement result changes
| Enum | Values |
|---|---|
| `EGP_MovementResult` | `Reached`, `Cancelled`, **`Failed`** |
| `EGP_MovementResultReason` | `None`, `Superseded`, `CommandReplaced`, `Manual`, **`PathNotFound`**, **`PathInvalid`**, **`DestinationOffNav`**, **`Blocked`** |
| `EGP_MovementRejectReason` | prior + **`PathNotFound`**, **`DestinationOffNav`** |

Consumers (`UGP_UnitCommandComponent` Move/AttackMove/Attack/Mine/Haul) treat `Failed` like cancel/fail paths — never as `Reached`.

## 6. Collision behavior
- Capsule: Visibility Block; **Pawn Overlap**; **WorldStatic Ignore**
- Soft unit footprint via overlap queries + separation; static obstacles via NavMesh
- Sweep enabled but soft channels do not hard-block crowds
- No physics simulation

## 7. Local avoidance / separation
- Bounded `OverlapMultiByChannel(ECC_Pawn)` within `SeparationRadius` (default 120)
- Lateral push scaled by `SeparationStrength` (1.25), capped by `MaxSteeringContribution` (0.55)
- Nav direction remains primary; no global actor scan / no flocking architecture

## 8. Group destination assignment
- `UGP_CommandComponent::DispatchValidatedCommand` for Move + AttackMove
- Deterministic compact grid (`GroupSlotSpacingCm=110`); 1 unit → exact click
- Each slot projected to nav; ring fallback; if still invalid → **keep Desired grid slot** (no center collapse)
- AttackMove resume uses each unit’s own Held slot destination

## 9. Tuning / defaults
| Property | Default |
|---|---|
| MoveSpeed / AcceptanceRadius / RotationSpeed / bRotateToMovement | preserved |
| NavProjectionExtentXY / Z | 250 / 400 |
| RepathIntervalSeconds | 0.75 |
| BlockedFailSeconds | 4.0 |
| SeparationRadius / Strength | 120 / 1.25 |
| MaxSteeringContribution | 0.55 |
| bRequireNavigationWhenAvailable | true |
| GroupSlotSpacingCm | 110 (command dispatch) |

## 10. Compatibility
| Consumer | Notes |
|---|---|
| Move | Unchanged public API; auto-acquire suppress preserved |
| Attack approach | Uses nav/fallback via same `RequestMove` |
| AttackMove | Per-unit spread slots; resume original Held destination |
| Mine / Haul | Same serial/result ownership; nav when on mesh |

## 11. Contract results
| Command | Failures |
|---|---|
| `gp.Movement.RunRTSMovementReconciliationContractTest` | **0** |
| `gp.Combat.RunAttackMoveContractTest` | **0** |
| `gp.Combat.RunAutoAcquireContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunContainerLaunchHUDContractTest` | **0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** |

## 12. Builds
- GPEditor Win64 Development + UHT **PASS**
- GP Development / Shipping **not run** (finalization gate)

## 13. Changed files (candidate)
- `GP/Source/GPRuntime/Public/Units/GPMovementComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPMovementComponent.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnit.cpp`
- `GP/Source/GPRuntime/Private/Units/GPWorker.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Command/GPCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Combat/GPRTSMovementReconciliationContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPRTSMovementReconciliationContractTest.cpp`
- Docs: GP-S33M task, AI_Project_Log, DOCUMENTATION_INDEX, Claude_Tasks README, TDD/04, TDD/05, this report

## 14. Config / map / content touched
**None committed.** Operator-local `DefaultEngine.ini` / `DefaultGame.ini` / `L_PrototypeArena.umap` / Blueprint / Materials / VFX / Tools left untouched.

## 15. Operator NavMesh setup (if needed)
1. Open `L_PrototypeArena` locally  
2. Add `NavMeshBoundsVolume` covering arena floor  
3. Build Paths  
4. Do not commit the umap for this slice  

## 16. PIE acceptance sketch
A. RMB Move behind large static obstacle → goes around  
B. 3–4 unit RMB same point → separated arrivals, no permanent overlap stack  
C. AttackMove → fight → each unit resumes own slot  

## 17. Known limitations / deferred
- Dynamic orbital buildings may not auto-carve nav (document only)
- No formation facing/persistence
- No MassAI / AIController-per-unit
- Hard Pawn↔Pawn blocking intentionally not used (deadlock avoidance)
- Isolation / off-nav diagnostic spawns use straight-line fallback by design
