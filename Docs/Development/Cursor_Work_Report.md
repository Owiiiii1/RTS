# Cursor Work Report — GP-S28P0 Resource Playable Pass Audit

## Status
**GP_S28P0_AUDIT_READY_FOR_REVIEW**

## Branch
`audit/gp-s28p-resource-playable-pass`

## Base
`main` @ `035c486758059032bb2551520834dd73f8667ef5`

## Parallel branch (untouched)
`audit/gp-slice7-combat-reconciliation` — completed combat audit, **pending review/merge** (not cancelled; not modified by this stage)

## Sources reviewed
Docs README, DOCUMENTATION_INDEX, AI_Project_Log; TDD/04,05,06,07,10,12,13; GDD/02,04,05,06; ADR-0002/0006/0007/0009; CONTRIBUTING; STYLE

## Code files inspected
- `GPCommandComponent` smart Mine + server Worker filter
- `GPPlayerController` command input / selection draw
- `GPUnitCommandComponent` Mine/Haul replace/interrupt
- `GPWorker`, `GPMainBase`, `GPBuildingBase`, `GPResourceNode`
- `GPCargoComponent`, `GPMiningComponent`, `GPStorageComponent`
- `GPGameState` MainBase registry / threat
- Visual/primitive helpers (Worker has Capsule only; Node has visual component)

## Current right-click Mine result
**Works today:** select Worker(s) → RMB ResourceNode → `GP.Command.Mine` → approach → BeginMining → haul loop. No modifier. Mixed selection keeps Workers only. Move cancels Mine/Haul (cargo kept).

## Current depletion behavior
Actor **remains** at 0; Mine rejected (`Depleted`); no auto-Destroy; no BP `OnResourceDepleted`; active miners go DepositDepleted / haul if cargo.

## Current FIFO behavior
Per-node `MaxConcurrentMiners=4`; excess → Waiting FIFO; promote on release; Worker moves into range before queueing; EndPlay clears without promote. **No** cross-node reassignment.

## Current drop-off behavior
Single MainBase per team via GameState registry → Storage add + Accepted-only threat; overflow LOST. LogisticsHub is **not** a drop-off (TDD).

## Blueprint readiness
Worker / MainBase / ResourceNode are `Blueprintable` concretes with Capsule/Box roots — BP children safe. Missing presentation anchors and cargo attach point in C++.

## Cargo visual readiness
`OnCargoAmountChanged` + OnRep sufficient for show/hide; no cargo actor needed.

## HUD source recommendation
**A′ — TEMP Planetary HUD** after client-safe MainBase resolve (registry today is authority-only). Value = `Storage.GetTotalStored()` for local team; not Score/Orbital. Optional later B-lite VM.

## Proposed architecture
- GameState ResourceNode registry (mirror MainBase)
- Path-aware search params; no GetAllActorsOfClass hot path
- Depleted persistent shell + events
- **No** `IGP_FerroniteDropOff` in S28P
- Event-driven waiting; no permanent Tick

## Proposed P1–P4 split
See `Resource_Playable_Pass_Audit.md` §12 (P3 slimmed to MainBase-only resilience).

## Operator responsibilities (post-implementation)
Create BP_GP_Worker / MainBase / ResourceNode_Ferronite; place TeamId 1/2; NavMesh; one MainBase/team; LFS map — checklist in audit §11.

## Blocking questions / conflicts
Storage-full LOST vs wait; depleted shell vs Destroy; keep combat audit separate; P4 needs client MainBase resolve; Shift-queue is QueueDeferred no-op (out of S28P unless assigned).

## Follow-up inventory note
Post-commit review from playable-path explore confirmed same core findings and added: QueueDeferred no-op, cargo-full Mine reject nuance, MiningComponent BP delegates, unreplicated MainBase registry for client HUD — folded into audit amend.

## Build result
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | not required (docs-only) |

## Files changed
- `Docs/Development/Resource_Playable_Pass_Audit.md` (created)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Cursor_Work_Report.md`

## Commit SHA
`d7710e8d7bda59793bc1c8c93363d58640465654`

## Git status
(to verify: clean; branch synced; main untouched)

## Status
**GP_S28P0_AUDIT_READY_FOR_REVIEW**
