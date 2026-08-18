# Cursor Work Report — GP-S40R Finalization

## Status
**GP-S40R_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head
- Branch: `feature/gp-s40r-timed-retaliation-pursuit`
- Base: `origin/main` @ `5ad69aa7abd39e181cd6ffafb11e4277adf3160a`
- Head: recorded after commit

## Operator
**PASS.** Retaliation behavior confirmed correct.

## Targeted regressions
Headless `UnrealEditor-Cmd` `-game -NullRHI` on `/Game/GrimProtocol/Maps/L_PrototypeArena`:

| Command | Result |
| --- | --- |
| `gp.Combat.RunRetaliationPursuitContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunLOSFireGateContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunAutoAcquireContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite: **NOT RUN**.

## Final builds
| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** (after Shipping-only contract-runner stubs; no gameplay change) |

## Final audit
- Notify only after successful hostile `ApplyDamageFromUnit` (living target)
- Duration sole source: `UGP_UnitDefinition.RetaliationPursuitSeconds` via `GetRetaliationPursuitSeconds()`
- Worker is not combat-capable → no autonomous combat retaliation
- DefensiveTurret / buildings excluded from movement retaliation
- Manual Held commands cancel/suppress retaliation
- In-sight + blocked LOS keeps retaliation-owned pursuit
- Clear LOS hands off once to existing Attack FSM
- Timeout returns Idle; no Held Attack left behind
- No second fire/damage path; Attack Ready LOS fire gate unchanged
- No new permanent Tick (timeout + 0.20s evaluate timers only)
- Protected operator assets / config / maps untouched

## Unrelated NEXT defect
[`Claude_Tasks/GP-S41M_Movement_Shortest_Yaw.md`](Claude_Tasks/GP-S41M_Movement_Shortest_Yaw.md)  
Operator: Move facing may take the long yaw path (~350° when ~10° would suffice). Root cause **not** claimed. **Not fixed in GP-S40R.**

## Merge
**NOT MERGED.** Await human merge onto `main`.
