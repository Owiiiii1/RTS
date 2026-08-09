# Cursor Work Report — GP-S32A Attack-Move Reconciliation

## Status
**GP-S32A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s32a-attack-move-reconciliation` |
| Base (`main`) | `989ca3fe6eae31b177ba2fade2ca1f02300d3326` |
| Candidate head | 222170ad18c0c7d1bc236e88523e3b938c6ec7e7 |

## 2. Factual input path
- `A` key edge (polled in Tick via `UpdateAttackMoveInputOwnership`) → `EnterAttackMoveMode` when selection has SalvageWalker
- Modal LMB click → `ConfirmAttackMoveDestination` → `Server_RequestCommand(AttackMove)`
- Esc / RMB → cancel modal (RMB suppress until release, same ownership idea as building placement, **not** coupled to placement code)
- Confirm LMB suppresses selection/marquee/click-through via modal ownership flags

## 3. Command validation / routing
- `UGP_CommandComponent::ValidateAndNormalizeCommand` accepts `GP.Command.AttackMove`
- Location-sane, TargetActor cleared; issuers filtered to `Unit_Type_SalvageWalker` (else `UnsupportedUnit`)
- `DispatchValidatedCommand` → `ReceiveCommand` → `HandleCommand` unchanged delivery path

## 4. AttackMove state representation
- Held command tag = `Command_AttackMove`; destination = `Held.TargetLocation`
- Destination travel via existing `SynchronizeMovementWithHeldCommand` / `RequestMove` (same as Move)
- Engagement: `StartAttackMoveEngagement` runs Attack FSM with `ActiveAttackSerial == Held.CommandSerial` **without** replacing Held
- `HasExactActiveHeldAttack` accepts Attack **or** AttackMove ownership

## 5. Resume-after-combat
- `FinishAttack` while Held AttackMove → `ResumeAttackMoveTravelAfterEngagement()` to original destination
- Explicit Move/Attack/Stop replace Held → no stale resume

## 6. Command replacement
| Incoming | Effect |
|---|---|
| Move | Abandon AttackMove; pure Move |
| Explicit Attack | Abandon AttackMove; engage target; no old dest resume |
| Stop | Clear Held + attack → Idle |
| New AttackMove | Replace destination/serial |

## 7. Worker / ineligible
- Server validate rejects Worker-only AttackMove (`UnsupportedUnit`)
- Mixed selection: only SalvageWalkers dispatched
- Worker never gains combat capability / AttackMove eligibility

## 8. New gameplay components?
**None.** Orchestration in `UGP_UnitCommandComponent` + PC modal + CommandComponent validate.

## 9. Builds
GPEditor Win64 Development + UHT: **PASS**  
GP Dev / Shipping: **NOT RUN**

## 10. Contracts / regressions — Failures=0
| Command | Result |
|---|---|
| `gp.Combat.RunAttackMoveContractTest` | **0** |
| `gp.Combat.RunAutoAcquireContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |
| `gp.Combat.RunHealthBarContractTest` | **0** |
| `gp.Combat.RunTeamColorContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunContainerLaunchHUDContractTest` | **0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** |

## 11. Changed files
- `GPCommandComponent.cpp` — AttackMove validate + SW filter
- `GPUnitCommandComponent.h/.cpp` — AttackMove travel/engage/resume; Idle acquire preserves Move suppress
- `GPPlayerController.h/.cpp` — A/Esc/LMB AttackMove modal
- `GPCombatAttackMoveContractTest.h/.cpp` — new contract
- Docs: task, TDD/04 minimal, AI log, DOCUMENTATION_INDEX, Claude_Tasks README, Cursor_Work_Report

## 12. Operator assets untouched
DefaultEngine/Game.ini, map, Blueprint/, Materials/, VFX, Tools/, `.uasset`/`.umap` — not committed.

## 13. Operator test sketch
1. Select SW → A → LMB ground beyond enemy → travel → fight → resume dest  
2. While fighting → RMB Move elsewhere → abandon AttackMove, obey Move  

## 14. NEXT
Operator validation only. Do **not** auto-start RTS Movement Reconciliation.
