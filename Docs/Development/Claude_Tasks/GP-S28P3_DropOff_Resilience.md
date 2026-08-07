# GP-S28P3 — Drop-Off Resilience (MainBase Haul Recovery)

## Status
**GP-S28P3_SPEC_READY_FOR_REVIEW**

## Slice Group
Slice 6 follow-on — Resource Playable Pass (P3)

## Code Allowed
**NO** — specification only. Implementation requires explicit approval after review.

## Depends On
- GP-S28 merged (haul / Storage / Threat)
- GP-S28P1 merged @ `86bcc9740fde0f19ac40c70f2f49298680f5f7d6`
- GP-S28P2 merged @ `e90b7bd48fb9080a881e6dda7be889eaa99a3161`
- Canonical: GDD/02, GDD/06, TDD/06, TDD/07, TDD/13, ADR-0009, `Resource_Playable_Pass_Audit.md`

## Goal
Make Worker haul/drop-off resilient when MainBase is missing, destroyed mid-haul, or temporarily unreachable — without inventing new drop-off buildings, storage redesign, HUD, or combat.

Cargo must never be silently abandoned solely because MainBase is unavailable. Mine intent / search-anchor continuity from P2 must survive WaitingForDropOff → successful unload → PostDropOff.

## Canonical gameplay (unchanged)
```
Mine → Cargo → MainBase containers → ThreatValue → later launch / orbital systems
```
- MainBase is the **only** MVP Ferronite drop-off.
- LogisticsHub remains storage/unit-cap related — **not** Ferronite drop-off (`bProvidesDropOff=false` per TDD/06).
- Threat increases only from **accepted** MainBase storage add (existing P2/S28 rule).

## Pillar / architecture check
| Constraint | P3 compliance |
| --- | --- |
| Orbital-delivery pivot (ADR-0009) | No local production/construction; planetary intake only at MainBase |
| MainBase-only planetary Ferronite intake (GDD/02, GDD/06) | No multi-drop-off / Hub drop-off |
| Threat = raw Ferronite in MainBase containers now | Threat only on successful Accept; never during wait/retry |
| GDD = WHAT, TDD/ADR = HOW | Spec defines recovery HOW within existing UnitCommand haul SoT |

---

## Actual code audit (main @ `e90b7bd…`)

### Haul state machine today
`EGP_HaulExecutionState` (`GPUnitCommandComponent.h`):
`Idle`, `ReturningToBase`, `DroppingOff`, `ReturningToDeposit`, **`WaitingForStorage`**, `Failed`

| Path | Current behavior |
| --- | --- |
| `StartHaulReturnToBase` + no MainBase/Storage | Log `MissingMainBaseOrStorage` → `Failed` → **`FinishHaulChain(true)`** (clears held Mine) |
| Approach move rejected | `Failed` → `FinishHaulChain(true)` |
| Drop-off out of range / team mismatch | `Failed` → `FinishHaulChain(true)` |
| Storage overflow (`Rejected > 0`) | `ClearCargo()` LOST + still `ContinueMineAfterSuccessfulHaul` |
| Successful Accept | Threat += Accepted × ThreatPerStoredUnit; PostDropOff / return-to-deposit / WaitingForResource (P2) |

**Gap:** cargo is not cleared on MissingMainBase fail, but **held Mine intent is cleared** — Worker cannot auto-resume haul when a base appears. No stable wait state is entered.

### WaitingForStorage
Present on haul + Worker activity enums and logs; **never assigned**. Name implies storage-full wait (not implemented; overflow = LOST). P3 must **not** activate it for storage capacity.

### MainBase registry
`AGP_GameState`: `RegisterMainBase` / `UnregisterMainBase` / `FindMainBaseForTeam` (authority, one per team, `TeamId >= 1`).
`AGP_MainBase`: register on BeginPlay / TeamId refresh; unregister on EndPlay / team clear.
**No** `OnMainBaseRegistered` / unregister multicast (unlike ResourceNode registry wake used by P2 WaitingForResource).

### Drop-off approach geometry
`RequestHaulApproachMove` → `TryMakeRangeApproachDestination` using MainBase **actor location**, `DropOffRangeCm` (default 400), movement AcceptanceRadius, `ResourceApproachSafetyMarginCm`.
`DropOffVisualAnchor` is presentation-only.
MainBase capsule half-extent is **not** currently fed into approach geometry (unlike ResourceNode CollisionBox). Acceptance criterion today: Worker within `DropOffRangeCm` of MainBase actor location.

### Settings (`UGP_ResourceGameplaySettings`)
Existing: search radius/path, WaitingForResource retry, depletion delay, approach safety/directions.
**No** drop-off wait retry parameter yet.

### P2 invariants to preserve
- Depletion + Cargo > 0 → haul first (`ReturnToDeposit=false`) before reassignment.
- Held Mine + MineSearchAnchor persist through haul → PostDropOff.
- WaitingForResource ⇒ Cargo=0 in normal flow.

---

## Problem statements (required P3 outcomes)

### A. MainBase temporarily missing
Worker Cargo > 0; `FindMainBaseForTeam` null.
- Cargo preserved
- Mine intent / search anchor preserved
- Enter **WaitingForDropOff** (not Failed, not clear held)
- No permanent Tick
- When valid MainBase registers for team → wake → haul

### B. MainBase destroyed while hauling
- Cancel current haul movement safely
- Clear stale MainBase weak ptr
- Cargo preserved; Mine intent preserved
- Enter WaitingForDropOff
- Replacement MainBase → auto-resume

### C. MainBase exists but path unavailable
- Cargo preserved
- No MoveFailed repath spam / tight loop
- WaitingForDropOff
- Event wake preferred; low-frequency safety retry allowed

### D. MainBase becomes reachable again
- Leave WaitingForDropOff → haul → unload
- Then P2 PostDropOff continuation (return / reassignment / WaitingForResource)

---

## Storage-full policy (explicit non-goal)
Confirmed on main: `AddPlanetaryFerronite` returns Rejected remainder; drop-off **`ClearCargo()` LOST** and continues mine chain.
P0 audit assumed overflow LOST. **P3 does not redesign this.**
`WaitingForDropOff` = missing / dead / unreachable MainBase only — **not** storage capacity.

---

## Proposed state model

### Recommendation
Reuse the unused haul enum slot by **renaming** `WaitingForStorage` → `WaitingForDropOff` on both:
- `EGP_HaulExecutionState`
- `EGP_WorkerActivityState`

Rationale: slot already exists, never entered; name “Storage” would confuse with overflow LOST. Do **not** add a parallel manager/subsystem.

If rename is rejected in review, add `WaitingForDropOff` and leave `WaitingForStorage` unused/deprecated — still no storage-full wait in P3.

### Canonical transitions
```
CargoFull / DepositDepleted(+Cargo>0)
  → ReturningToBase (haul)
      → DroppingOff → PostDropOff continuation (P2)
      → WaitingForDropOff  (missing / destroyed / unreachable)
          → ReturningToBase  (wake / retry success)
          → Idle/cleared     (explicit command replacement)
```

Partial-cargo depletion path:
```
DepositDepleted + Cargo>0
  → ReturningToBase (ReturnToDeposit=false)
  → WaitingForDropOff if base problem
  → DropOff
  → PostDropOff reassignment / WaitingForResource
```

### Forbidden transitions
- WaitingForDropOff → clear Cargo
- WaitingForDropOff → Threat mutation
- MissingMainBase → `FinishHaulChain(true)` clearing Mine intent (current behavior — must change)

---

## Ownership / SoT

| Concern | Authoritative SoT |
| --- | --- |
| MainBase presence per team | `AGP_GameState` MainBase registry |
| Haul / WaitingForDropOff | `UGP_UnitCommandComponent` (authority) |
| Cargo | `UGP_CargoComponent` |
| Storage Accept/Reject | `UGP_StorageComponent` on MainBase |
| Threat | `AGP_GameState` on accepted drop-off only |
| Mine intent / search anchor | UnitCommand held Mine + MineSearchAnchor (P2) |
| Movement | `UGP_MovementComponent` results consumed by UnitCommand |

### Subscription SoT (to add in implementation) — two distinct purposes

Do **not** bind all MainBase registry events only while WaitingForDropOff. That would miss destruction during active haul (`ReturningToBase` / `DroppingOff`).

Add GameState multicasts (authority), mirroring ResourceNode registry pattern:
- `OnMainBaseRegistered(AGP_MainBase*)`
- `OnMainBaseUnregistered(AGP_MainBase*)`

#### 1. Active-haul interruption (current target lifecycle)

While `HaulState == ReturningToBase` **or** `DroppingOff` and a concrete haul target MainBase is set:

| Preferred option | Rationale |
| --- | --- |
| **A. Bind `AGP_GameState::OnMainBaseUnregistered`** during active haul | Matches existing UnitCommand ↔ GameState ResourceNode registry wake pattern; no per-actor EndPlay plumbing today |
| B. Bind EndPlay/lifecycle on the target MainBase | Acceptable if implementation audit finds it safer; not the default preferred pattern |

On unregistered/destroyed event:
- Filter: event MainBase **must be** the current haul target (same object). Ignore other TeamId / other MainBase / stale previous targets.
- Cancel current haul movement
- Clear stale MainBase weak ptr
- Preserve Cargo, held Mine intent, MineSearchAnchor
- Enter WaitingForDropOff
- Arm waiting wake (`OnMainBaseRegistered`) + drop-off retry timer

Clear active-haul lifecycle subscription on:
- successful drop-off
- entering WaitingForDropOff (swap to waiting-registration bind)
- command replacement
- haul cancel / failure replacement paths that leave haul
- UnitCommand EndPlay

No duplicate bindings (at most one active-haul unregister handle per UnitCommand).

#### 2. Waiting wake (registration only)

While `HaulState == WaitingForDropOff` **only**:
- Bind `OnMainBaseRegistered`
- On valid same-team MainBase: wake **exactly once** → unsubscribe registration delegate → clear retry timer → attempt haul
- If attempt still unrecoverable/unreachable: return to stable WaitingForDropOff (no recursive immediate retry)

#### 3. Unregister while already waiting

`OnMainBaseUnregistered` while WaitingForDropOff is **not** required for wake.
After team/target filtering: **no-op** (no new transition / no retry storm).

### State / event table

| State | Event | Result |
| --- | --- | --- |
| ReturningToBase | current MainBase unregistered/destroyed | cancel move → WaitingForDropOff |
| DroppingOff | current MainBase invalidated before transaction | WaitingForDropOff; Cargo intact |
| WaitingForDropOff | valid same-team MainBase registered | retry haul once |
| WaitingForDropOff | unrelated MainBase register/unregister | no-op |
| any haul/wait | explicit command replacement | cleanup all haul/wait subscriptions + timer; preserve Cargo |

### Movement failure remains fallback
`MoveFailed` / path rejection for an existing but unreachable MainBase → WaitingForDropOff.
Destruction detection must **not** depend only on eventual movement failure when registry/lifecycle notification is available.

---

## Retry policy
- No permanent Actor/Component Tick for wait.
- Primary while waiting: registry **register** wake.
- Secondary: low-frequency safety retry for unreachable→reachable without registry churn.

**Settings:** extend existing `UGP_ResourceGameplaySettings` (no new settings class):

| Property | Proposed default | Notes |
| --- | --- | --- |
| `DropOffRetrySeconds` | `3.0f` | Align with `WaitingForResourceRetrySeconds`; clamp ≥ 0.1 |

No hardcoded timing in Worker.

Suppress duplicate no-candidate / unreachable log spam (same pattern as WaitingForResource).

---

## Path / drop-off acceptance semantics
- Gameplay target remains **range to MainBase**, not `DropOffVisualAnchor`.
- Worker must finish inside valid **`DropOffRangeCm`** interaction (existing BeginDropOff checks).
- Do not require actor origin occupancy.
- Implementation must verify approach remains safe with MainBase capsule/nav (same class of bug fixed for ResourceNode in P2). If current actor-location + DropOffRange approach already yields in-range arrivals in PIE, do not rewrite geometry without evidence.
- Acceptance criterion for P3: after approach `Reached`, `BeginDropOffAtMainBase` succeeds without corrective loop spam; Cargo unload uses existing Storage path.

---

## Command replacement (active haul or WaitingForDropOff)
On Move / Attack / Stop / replacing command:
- Cancel pending haul / wait
- Unsubscribe active-haul unregister binding (if any)
- Unsubscribe waiting-registration wake (if any)
- Clear drop-off retry timer
- **Preserve Cargo**
- Clear/replace held Mine intent per existing UnitCommand accept semantics
- Must **not** allow a later registry wake to resume the cancelled haul

---

## Multiplayer / authority
- All wait/wake/haul/path/Cargo/Storage/Threat transitions: **server authority only**.
- No client MainBase search.
- Replication only for existing presentation/activity mirroring if already required — no P4 HUD scope.

---

## Out of scope
- Storage-full redesign / wait-on-full queue
- Overflow LOST policy redesign
- Multiple Ferronite drop-off points
- LogisticsHub drop-off
- Generic `IGP_DropOff` / provider interface
- Cargo actor replication redesign
- HUD / MVVM / client presentation pass (P4)
- Orbital launch / Score / OrbitalFerronite spend
- Combat / construction
- Command queue execution
- Save/load recovery

---

## Required contract test (future implementation)

`gp.Resource.RunDropOffResilienceContractTest`

| # | Case | Expect |
| --- | --- | --- |
| 1 | MainBase missing, Cargo > 0 | WaitingForDropOff; Cargo unchanged; held Mine kept |
| 2 | MainBase registers | Wake once; haul; unload succeeds |
| 3 | Base destroyed mid-haul | Observed while `ReturningToBase`; movement cancelled; WaitingForDropOff **exactly once**; Cargo unchanged; **not** dependent on waiting retry timer to notice destruction |
| 4 | Replacement MainBase | Registration wake **exactly once**; deliver |
| 5 | Unreachable Base | Stable wait; no rapid retry/log spam |
| 6 | Move while waiting | Obeys Move; old wake cannot resume haul |
| 7 | Partial cargo depletion + base missing | P2 haul-first; cargo not stranded; wait then deliver |
| 8 | Threat | Increases only by Accepted; never during wait/retry |
| 9 | Subscriptions | One Worker / one haul transition: no duplicate lifecycle or registry bindings; no duplicate timers |
| 10 | Regression | `gp.Resource.RunS28RegressionSuite` Failures=0; P2 suite Failures=0 |

Also extend suite entry after P3 lands.

## Operator validation plan (future PIE)
| | Scenario |
| --- | --- |
| A | Full cargo, no MainBase → wait with cargo; place MainBase → auto-deliver |
| B | Destroy MainBase mid-haul → wait, cargo intact; spawn replacement → deliver |
| C | Block path to MainBase → stable WaitingForDropOff, no jitter/spam; restore path → resume |
| D | While waiting, issue Move → obeys Move; never auto-resumes old haul |

## Builds (implementation phase — not this docs task)
GPEditor Dev+UHT; GP Development; GP Shipping — all PASS before READY_FOR_MERGE.

## Stop condition (this docs task)
Spec reviewed. Do **not** implement until explicit Code Allowed approval.
