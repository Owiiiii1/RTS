# Cursor Work Report — GP-S30R Combat Auto-Acquire

## Status
**GP-S30R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s30r-combat-auto-acquire` |
| Base (`main`) | `ba98383ffca90dafc4645b8761bfaeb93fa5cdc2` |
| Candidate head | e4529b4fa8cf512f47d6ada48d194031fb3ebb4f |

## 2. Architecture
- Integrated into **`UGP_UnitCommandComponent`** (no new Targeting/Combat component)
- Why: Idle auto-engage is a thin command-layer scan → existing `HandleCommand(Attack)` / Attack FSM; separation into a new component not justified
- Combat-capable: `HasCapabilityTag(Unit_Type_SalvageWalker)`
- Scan: looping authority timer, default **0.35s** (`AutoAcquireScanIntervalSeconds`)
- Selection: nearest 2D distance within `GetAttackRange()` (GAS), tie-break lexicographic actor name; **buildings excluded**
- Validation: reuse `ValidateAttackTarget` (alive, enemy team, not self)

## 3. Command priority
- Explicit Attack → not overridden by nearer targets while Held/active
- Move → cancels attack; suppresses auto-acquire while Move held / moving
- Stop → clears Held + attack/move; Idle may resume scan
- Auto path never creates a parallel fire/damage implementation

## 4. Target loss
Existing Attack FSM TargetDied / invalid clears Held; subsequent Idle scans may acquire another living enemy. No sticky invalid target.

## 5. Builds
GPEditor Win64 Development + UHT: **PASS**  
GP Dev / Shipping: **NOT RUN**

## 6. Contracts / regressions (NullRHI `-game`) — Failures=0
| Command | Result |
|---|---|
| `gp.Combat.RunAutoAcquireContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |
| `gp.Combat.RunHealthBarContractTest` | **0** |
| `gp.Combat.RunTeamColorContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** |

## 7. Changed files
- `GPUnitCommandComponent.h/.cpp` — auto-acquire + Stop
- `GPCommandComponent.cpp` — accept `Command_Stop`
- `GPCombatAutoAcquireContractTest.h/.cpp` — new contract
- Docs: task, AI log, DOCUMENTATION_INDEX, Claude_Tasks README, Cursor_Work_Report

## 8. Operator assets untouched
DefaultEngine/Game.ini, map, Blueprint/, Materials/, VFX, Tools/, `.uasset`/`.umap` — not committed.

## 9. Operator test sketch
Idle SW + enemy in range → auto Attack; Move → obey Move (no continued auto combat).

## 10. Next
Pending operator validation only. Do **not** auto-assign Attack-Move.
