# Cursor Work Report — GP-S27 Worker Mine Approach Range Correction

## Task
GP-S27 — Worker Mine Approach Range Correction

## Status
**GP-S27_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s27-worker`

## Base
`main` @ `860070c4acbcb85fd5c4334628584372bdd082ca`

## Candidate
`07e20fbfff36e181076d237d0596ef6f25b40951`

## Correction commit
*(recorded after commit)*

## Operator manual reproduction
Worker ~4563.6 cm from ResourceNode → Mine approach Destination≈(1363,-1155,88) → MoveReached Final≈(1381,-1108,88) → Distance=**200.4** Range=**200.0** → `MineArrivalOutOfRange`. Contract test previously passed with Dist≈192.7 (missed edge).

## Root cause
Movement completes anywhere inside a **2D AcceptanceRadius (50)** circle around destination, while mining validates **3D** `FVector::Dist`.

Old formula placed destination at horizontal:
`D_h = Range − Acc − 5 = 145`.

Worst-case flat: `145+50=195 < 200`, but with **ΔZ** between Worker and Node:
`worst = sqrt((D_h+Acc)² + ΔZ²)` exceeds 200 (operator ~200.4). Dest also used Worker Z, so destination itself was already farther in 3D than D_h.

## Old formula
`Node + normalize2D(Worker−Node) * (Range − AcceptanceRadius − 5)`, Z=Worker.Z

## New formula
Require `sqrt((D_h + Acc)² + ΔZ²) < Range`:
`D_h = sqrt(Range² − ΔZ²) − Acc − WorkerMineApproachSafetyMarginCm(25) − ExtraInward`

Private constexpr `WorkerMineApproachSafetyMarginCm = 25.f`. Typical flat: D_h≈125, PredictedWorst≈175.

If `|ΔZ|` consumes the budget → typed geometry failure (no doomed move).

## Acceptance-radius proof
Movement Reached uses 2D Acc. PredictedWorstCaseDistance = `sqrt((D_h+Acc)²+ΔZ²)` asserted `< InteractionRange` before RequestMove. Mining range / BeginMining OOR unchanged (strict).

## Vertical-distance policy
Horizontal budget shrinks by ΔZ; impossible ΔZ rejects approach. MiningComponent stays 3D.

## Corrective retry policy
On arrival OOR: at most **one** deeper corrective `RequestMove` (same command serial, Attempt=1, extra inward margin). No slot/timer until success. Second OOR → clear Held / fail. No Tick polling.

## Stale callback protection
Still keyed on `ActiveMineSerial` / Held.CommandSerial; replace resets Mine executor (attempt + diagnostics).

## Contract edge tests
- `ApproachWorstCaseWithinRange` / safety margin
- `ApproachVerticalBudgetWithinRange`
- `DiagonalApproachMarginSafe` (+ arrival margin)
- Corrective forced OOR once → mining after Attempt≥1
- Existing immediate/interrupt/FIFO/CargoFull/deplete/EndPlay/Cargo regression retained
- Movement wait: one-shot prerequisite log; **time-based** timeout (20s); sparse progress logs

## Worker contract post-fix result
**Complete Failures=0** (Editor alive until Complete)

## Cargo regression
Invoked from runner (`gp.Cargo.RunContractTest`)

## Mining regression status
Not nested (staged async); production Mining range untouched — operator may re-run `gp.Mining.RunContractTest`

## Files changed
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Units/GPWorker.h`
- `GP/Source/GPRuntime/Private/Units/GPWorker.cpp`
- Docs: GP-S27 task, AI log, Cursor report

## GPEditor / UHT
**PASSED**

## GP Development / Shipping
**not run**

## Map / LFS
Unchanged

## No scope expansion
No Storage/ThreatValue/Worker BP/map/combat/projectiles/GP-S28

## Git state
`feature/gp-s27-worker` only; no main/PR/merge
