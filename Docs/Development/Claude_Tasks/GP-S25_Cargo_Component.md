# GP-S25 — UGP_CargoComponent

## Status
**GP-S25_FINALIZED_READY_FOR_MERGE**

## Baseline
`main` @ `1fedf1933ac406c3a53a89af4a92a03afcf5a646` (GP-S24R merged)

Branch: `feature/gp-s25-cargo-component`  
Candidate: `f440838bbcd8963c8230a70f6f7e3363af7dc45a`  
Docs SHA note: `9b633115069b57266f209a829ac8254d60e032ea`

## Canonical roadmap position
Slice-6 coding stage:

`GP-S23R` → `GP-S24R` → **GP-S25 CargoComponent** → GP-S26 MiningComponent → GP-S27 Worker → GP-S28 Storage+ThreatValue

## Component ownership
- Class: `UGP_CargoComponent : UActorComponent` in GPRuntime
- Owner-agnostic reusable component
- Not permanently attached to combat units / `AGP_UnitBase`
- `AGP_Worker` (GP-S27) will own it
- Diagnostic host: transient `AGP_CargoDiagnosticHost` via `gp.Cargo.SpawnDiagnosticHost` (NotPlaceable; do not save to maps)

## Resource identity
| Layer | Value |
| --- | --- |
| Soft definition | `DA_GP_Resource_Ferronite` |
| Internal type | `EGP_ResourceType::Ore` |
| Player-facing | Ferronite (definition DisplayName) |
| Tag | `GP.Resource.Type.Ferronite` |

No competing DisplayName on the component. Capacity is cargo/unit tuning — **not** from ResourceDefinition.

## Exact state
| Field | Notes |
| --- | --- |
| `CargoCapacity` | EditDefaultsOnly, replicated; prototype **50** (TDD CarryCapacity / MaxCargo) |
| `CurrentCargoAmount` | ReplicatedUsing OnRep; default **0**; clamped `[0, CargoCapacity]` |
| Derived | RemainingCapacity, FillRatio, IsEmpty, IsFull |

## Source-of-truth decision
**`UGP_CargoComponent` is the sole writable runtime SoT** for temporary carried Planetary Ferronite.

No GameplayEffect cargo transactions. No OrbitalFerronite / FerroniteScore / Storage mutation from cargo.

## Legacy CarriedFerronite reconciliation
| Finding | Resolution |
| --- | --- |
| `UGP_UnitAttributeSet::CarriedFerronite` existed, replicated, unused | **Removed** from AttributeSet + replication + clamps |
| Dual writable stores | Eliminated |
| Compatibility mirror | Not kept |

## Mutation API (authority-only)
| API | Semantics |
| --- | --- |
| `AddCargo(Requested)` | Returns exact accepted (overflow clamp) |
| `RemoveCargo(Requested)` | Returns exact removed (clamp to current) |
| `ClearCargo()` | Returns removed amount |
| `CanAcceptCargo(Amount)` | Finite > 0 and remaining > 0 |
| Getters | Capacity / Current / Remaining / FillRatio / Empty / Full |

Rejects negative / zero / NaN / Inf safely; no public setter for current amount; no client mutation.

## Replication
- `CurrentCargoAmount` + `CargoCapacity` replicated
- ResourceDefinition soft not replicated (shared Ferronite default)
- No client-owned mutation RPCs
- Authority `ApplyCargoAmount` broadcasts once; `OnRep` fires on remotes only (no double-fire on local server mutation)
- `OnCargoAmountChanged` (Previous, New, Capacity, Delta)

## Delegates / notifications
BlueprintAssignable `OnCargoAmountChanged`. Fired on authority apply and client RepNotify. No Tick polling.

## Authority
All mutation requires owner authority. Diagnostic Add/Remove/Clear/Spawn/RunContractTest reject on client.

## Validation
`ValidateCargoContract` + editor `IsDataValid`: definition assigned/resolves, type/tag, capacity > 0 finite, current in range. No validation tick.

## Diagnostics (non-shipping)
| Command | Purpose |
| --- | --- |
| `gp.Cargo.SpawnDiagnosticHost` | Transient replicated host |
| `gp.Cargo.Inspect [Owner]` | Full state dump |
| `gp.Cargo.Add / Remove / Clear` | Authority mutation |
| `gp.Cargo.RunContractTest` | Deterministic contract (capacity 50 semantics) |

## Tick policy
`PrimaryComponentTick.bCanEverTick = false`. No timers. No polling.

## Operator validation matrix (accepted)

| Item | Result |
| --- | --- |
| Default Cap=50 Current=0 Empty Remaining=50 Fill=0 | **PASS** |
| Ferronite soft / PrimaryAssetId / Ore / tag | **PASS** |
| ValidationOk Errors=0 Warnings=0 | **PASS** |
| Add 30 → 30; Add 30 → Accepted 20 full; Add while full → 0 | **PASS** |
| Remove 30 → 20; Remove 100 → Removed 20 empty | **PASS** |
| Invalid 0/neg / NaN / Inf rejected; no mutation | **PASS** |
| Clear after Add 25 → Removed 25 | **PASS** |
| RunContractTest Failures=0 (all listed checks) | **PASS** |
| Listen server/client Cap/Current/Fill match; client Add rejected | **PASS** |
| Component/Actor tick disabled | **PASS** |
| Transient host; map/content unchanged | **PASS** |

## Builds (finalization)
- GPEditor Dev+UHT: retained from candidate (no C++ changes at finalization)
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

## In scope
CargoComponent; replicated state; Ferronite identity; add/remove/clear; validation; CarriedFerronite removal; diagnostics; diagnostic host; docs.

## Out of scope
MiningComponent; Worker; mining execution; ResourceNode consumption; occupancy; movement; Storage; MainBase; ThreatValue; Orbital/Score; UI; map; projectiles; visuals; cargo on every unit.

## Acceptance criteria
- [x] Component replicates; no tick
- [x] Capacity 50; empty default; clamp/overflow/remove semantics
- [x] Ferronite soft definition resolves
- [x] CarriedFerronite gone from AttributeSet
- [x] Client mutation rejected
- [x] Contract test + inspect diagnostics work
- [x] Map unchanged; no permanent unit attachment
- [x] GPEditor Dev+UHT passed; GP Dev/Shipping passed at finalization

## Known limitations
- No Worker ownership yet
- Capacity 50 is TDD prototype (may move to UnitDefinition later)
- Single-resource Ferronite only
- Diagnostic host is transient/debug-only

## Next canonical stage
**GP-S26 — UGP_MiningComponent**

No known blockers. Ready for main merge when requested.
