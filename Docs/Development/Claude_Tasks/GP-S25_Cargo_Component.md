# GP-S25 — UGP_CargoComponent

## Status
**GP-S25_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `1fedf1933ac406c3a53a89af4a92a03afcf5a646` (GP-S24R merged)

Branch: `feature/gp-s25-cargo-component`

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

No GameplayEffect cargo transactions. No OrbitalFerronite / FerroniteScore mutation from cargo.

## Legacy CarriedFerronite reconciliation
| Finding | Resolution |
| --- | --- |
| `UGP_UnitAttributeSet::CarriedFerronite` existed, replicated, unused by any gameplay/UI code | **Removed** from AttributeSet + replication + clamps |
| Dual writable stores | Eliminated |

Docs elsewhere may still mention CarriedFerronite historically; runtime SoT is CargoComponent only.

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
- `OnRep_CurrentCargoAmount` + `OnCargoAmountChanged` (Previous, New, Capacity, Delta)

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

## In scope
CargoComponent; replicated state; Ferronite identity; add/remove/clear; validation; CarriedFerronite removal; diagnostics; diagnostic host; docs.

## Out of scope
MiningComponent; Worker; mining execution; ResourceNode consumption; occupancy; movement; Storage; MainBase; ThreatValue; Orbital/Score; UI; map; projectiles; visuals; cargo on every unit.

## Acceptance criteria
- [ ] Component replicates; no tick
- [ ] Capacity 50; empty default; clamp/overflow/remove semantics
- [ ] Ferronite soft definition resolves
- [ ] CarriedFerronite gone from AttributeSet
- [ ] Client mutation rejected
- [ ] Contract test + inspect diagnostics work
- [ ] Map unchanged; no permanent unit attachment
- [ ] GPEditor Dev+UHT passed; GP Dev/Shipping deferred

## Operator validation
Capacity prototype = **50** (not 100).

1. PIE: `gp.Cargo.SpawnDiagnosticHost` then `gp.Cargo.Inspect` — empty, Cap=50, Current=0
2. `gp.Cargo.Add 30` → Current=30
3. `gp.Cargo.Add 30` → Accepted=20, Current=50, IsFull=true
4. Invalid inputs (neg/0) → accepted 0, no mutation
5. `gp.Cargo.Remove 20` → Current=30
6. `gp.Cargo.Remove 100` → Removed=30, Current=0
7. `gp.Cargo.Clear` after refill
8. `gp.Cargo.RunContractTest` — Failures=0
9. Listen server: Add on server; client Inspect shows same Current/Fill; client Add rejected
10. ComponentTickEnabled=false; do not save map

## Known limitations
- No Worker ownership yet
- Capacity 50 is TDD prototype (may move to UnitDefinition later)
- Single-resource Ferronite only
- Diagnostic host is transient/debug-only

## Next canonical stage
**GP-S26 — UGP_MiningComponent**
