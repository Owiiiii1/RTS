# Orbital Delivery System

## Scope

Engineering implementation of orbital drop pods (per [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md)). Defines three procurement flows (units, READY buildings, Wall Package), shared DropPod actor, validation, replication, presentation hooks, subsystem ownership. Replaces pre-pivot Production/Construction/GhostBuilding. `AGP_Wall` surface placement is **not** an orbital pod path (GP-S42C).

> **Owner refinement (2026-08-08):** See GDD/10. Units → MainBase Unit Drop Zone + transport-slot manifests. Buildings → Purchase/READY inventory then Deploy (no second spend). Shared rocket visual family; authored BP for mesh/Niagara.
>
> **GP-0305R (2026-08-18):** Wall Package is a **third flow**: spend → one rocket to MainBase → Wall inventory 0..5. Not READY. Not per-segment pods. [`../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md`](../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md).

## Hard Rules

1. **Non-initial material originates from orbit.** No local production / Worker construction. Units and READY buildings arrive via DropPod. Wall **Package** arrives via DropPod to MainBase. **`AGP_Wall` segments are not DropPod payloads** — they are placed from MainBase inventory.
2. **Server-authoritative.** Client intent only. `UGP_OrbitalDeliverySubsystem` validates and schedules pods for units / READY buildings / Wall Package. Surface wall placement is GP-S42C / BuildGrid, not this subsystem.
3. **Three orbital flows, one delivery actor family.** Unit manifests, building deploys, and Wall Package share `AGP_DropPod` + presentation BP; differ in targeting + payload data. Per-segment wall pods are **forbidden**.
4. **Units do not free-place.** Landing = server-resolved MainBase **Unit Drop Zone** (authored anchor — not hardcoded `BaseLocation + offset`).
5. **Buildings: spend on Purchase, not on Deploy.** Deploy consumes READY inventory once.
6. **Telegraph 2–3 s** (data-driven). Visible to all clients.
7. **No client prediction** of spend / inventory / payload.
8. **DataAsset-driven** per-purchase costs, slot costs, and delivery timing on drop definitions. Global pod capacity / altitude / spacing / cleanup / placement stay on `UGP_OrbitalDeliverySettings`.
9. **GAS spend only** via Instant spend GE (no direct attribute mutate). Spend exactly once on accepted unit Confirm / building Purchase / Wall Package purchase.
10. **FoW + grid** remain canonical for **building** placement when those systems exist. Unit Drop Zone path does not require FoW click targeting.

## Architecture (refined)

```
UNIT FLOW
─────────
TEMP/Order UI (manifest)
  → Server_RequestUnitDrop(Manifest)
  → Validate: Orbital, slots, MaxUnits, UnitDropZone
  → GE_SpendOrbital(TotalCost) once
  → Spawn AGP_DropPod → Landing = MainBase.UnitDropZone
  → OnLanding: spawn units with deterministic offsets + SetTeamId

BUILDING FLOW
─────────────
Order UI Purchase(DropDef)   // UGP_OrbitalDropDefinition / FPrimaryAssetId
  → Validate catalog + associated BuildingDefinition
  → GE_SpendOrbital(DropDef.Cost) once
  → READY[DropDefId]++

Deploy mode (ghost)
  → Server_RequestBuildingDeploy(DropDef, Loc, Rot)
  → Server snaps Loc to BuildGrid; validate READY + footprint free/unreserved + radius + nav/world
  → Reserve cells → spawn AGP_DropPod at snapped Loc (yaw 0) → READY[DropDefId]-- exactly once
  → OnLanding: spawn BuildingDefinition.SpawnedClass with OriginCell/FootprintSize
  (NO second Orbital spend)
```

WALL PACKAGE FLOW
─────────────────
Order UI BuyWallPackage(PackageDef)   // UGP_WallPackageDefinition
  → Validate stock==0, not in-flight, catalog, Orbital >= Cost
  → GE_SpendOrbital(PackageDef.Cost) once
  → Mark delivery pending; spawn AGP_DropPod → Landing = owning MainBase
  → OnLanding: WallSegmentInventory += SegmentCount (5) if capacity allows
  → Presentation: WallInventoryChanged(NewCount)
  (NO READY. NO placement mode. NO AGP_Wall spawn on landing.)

Build Wall is **not** an orbital RPC and does **not** spawn a DropPod. GP-S42C owns/routes surface `AGP_Wall` placement through wall / BuildGrid authority and consumes MainBase inventory.

Shared presentation: native DropPod lifecycle + soft `BP_DropPod_MVP` (or equivalent) for mesh/Niagara.

## MainBase Unit Drop Zone

Requirement (implementation class name TBD):

- Authored relative to MainBase (scene component / socket / dedicated component).
- Server resolves world transform for delivery.
- Replicated delivery uses that authoritative point.
- Owner relocates pad in BP without C++ offset rewrite.

## Transport Slots vs Unit Cap

| System | Role |
| --- | --- |
| `PodTransportSlotCapacity` | One pod packing limit (DA / settings). MVP example **4**. |
| `UnitTransportSlotCost` | Per unit type (Worker example **1**, Salvage Walker **2**). |
| `MaxUnits` / `CurrentUnits` | Army/workforce cap. |

Manifest rule: `sum(count × slotCost) <= PodTransportSlotCapacity`.  
Unit-cap MVP: **reject whole manifest** if it would exceed free cap — no silent partial fill.

## UGP_OrbitalDeliverySubsystem

Owns enqueue for **unit**, **READY building**, and **Wall Package** orbital flows, active pods, validation helpers, READY building inventory mutation (or delegates to a minimal owner-only replicated component on PlayerState — pick smallest server-authoritative structure at impl). Does **not** instantiate `AGP_Wall` segments.

Conceptual APIs (names illustrative):

- `TryEnqueueUnitManifest(Requester, Manifest, OutReason)`
- `TryPurchaseBuilding(Requester, BuildingDropDef, OutReason)`
- `TryDeployBuilding(Requester, BuildingDropDef, Loc, Rot, OutReason)`
- `TryPurchaseWallPackage(Requester, PackageDef, OutReason)`

Reject reasons should distinguish: InsufficientOrbital, ManifestExceedsSlots, UnitCapReached, MissingDropZone, NoReadyInventory, InvalidPlacement, etc.

## AGP_DropPod (shared)

Native responsibilities:

- Authority lifecycle, replication, descent timing/progress, landing complete, payload spawn, cleanup.
- Presentation hooks (conceptual): OnDescentStarted, descent progress/transform, OnImpact, OnPayloadDeploy.
- Soft ref to authored visual class/BP; **no hardcoded Niagara/mesh paths in gameplay C++.**

Authored BP owns: rocket StaticMesh, exhaust Niagara, impact/smoke Niagara, scale/offsets.

Payload timing MVP: **authority spawn once at landing-complete**; impact FX may mask reveal; optional short cosmetic delay.

Multi-unit: deterministic server offsets around landing point; stable ordering; basic spacing; no formation pathfinding.

## Data model sketches (conceptual)

**Unit line / catalog entry:** OrbitalCost, TransportSlotCost, soft unit class / UnitDef.

**Pod settings:** PodTransportSlotCapacity, DescentDuration, SpawnAltitude, visual soft class.

**Unit manifest:** list of (UnitDropDef or UnitType, Count) with derived total cost/slots.

**Building inventory:** replicated array of `{DropDefinitionId (FPrimaryAssetId), ReadyCount}` — owner-only; server decrements the matching DropDef on accepted deploy. Not keyed by `EGP_OrbitalBuildingType` (deprecated glue only).

**Building DropDef (`UGP_OrbitalDropDefinition`):** `Cost` (OrbitalFerronite) + `DropTags` + soft `BuildingDefinition`. Purchase cost only (deploy free of Orbital). Does **not** own spawned class or footprint.

**Building Definition (`UGP_BuildingDefinition`):** intrinsic `SpawnedClass`, `FootprintCells`, identity, soft `UnitDefinition`. `MaxHealth` is compatibility fallback; canonical MaxHealth is `UnitDefinition.MaxHealth` (GP-S38D). GP-S36G BuildGrid reads `FootprintCells` from here.

Single-payload DropDef for buildings; multi-payload carried as **manifest on the order**, not N separate pods unless future design says otherwise. MVP: **one pod per confirmed unit order**; **one pod per READY building deploy**; **one pod per Wall Package**. **Not** one pod per wall segment.

## Validation

### Unit Confirm

1. Valid playable TeamId + MainBase + Unit Drop Zone.
2. Manifest non-empty; slots OK; Orbital >= total cost.
3. `CurrentUnits + PendingOrbitalUnitCount + ManifestUnitCount <= MaxUnits` (always; MaxUnits == 0 is not unlimited). Full manifest reject (`UnitCapReached`). Entity count, not transport slots.
4. Reserve manifest entity count once → Spend once → spawn pod at Drop Zone. Payload live converts reservation to CurrentUnits. Failed payload releases leftover reservation.

### Building Purchase

1. DropDef valid; associated BuildingDefinition loaded; Orbital >= `DropDef.Cost`.
2. Spend once from DropDef.Cost → READY[DropDefId]++.
3. Cost is never read from BuildingDefinition.

### Building Deploy

1. READY[requested DropDefId] > 0.
2. Placement valid: finite transform; MainBase radius on **server-snapped** XY; `FootprintCells > 0`; all footprint cells free/unreserved; NavMesh MVP; environmental overlap sanity. **FoW placement validation deferred to FoW integration slice.** Server ignores unsnapped client precision and reconstructs OriginCell itself.
3. Reserve footprint → spawn pod at snapped location (yaw 0) → decrement READY[DropDefId] once → payload class from BuildingDefinition.SpawnedClass (Logistics Hub may fall back to deprecated settings `BuildingPayloadClass` / native `AGP_LogisticsHub`). Payload receives OriginCell + FootprintSize before BeginPlay (`ConfigureGridPlacement` rectangular path). Failed/skipped payload releases the reservation. Pre-placed buildings occupy from live oriented `PlacementFootprintBounds`; orbital spawn remains yaw-0 rectangular.
4. Cancel placement: no inventory change, no spend.
5. **GP-S33C / GP-S39E:** Logistics Hub `UGP_GE_UnitCap_Plus5` applies when the **payload building is live/operational**. Magnitude is SetByCaller from `BuildingDefinition.UnitCapBonus` (Hub +5). Editor-placed owned live Hubs also grant the bonus once.

## GP-S39E acquisition ownership

| Concern | Canonical |
| --- | --- |
| Worker / Walker cost + slots + payload + descent/deploy | `UGP_OrbitalUnitDropDefinition` (native: 25/1 and 50/2, 2.5+1.25) |
| Building cost | `UGP_OrbitalDropDefinition.Cost` (100 / 150 / 25 / 75) |
| Building descent/deploy | `UGP_OrbitalDropDefinition` (2.5 + 2.0) |
| Pod slot capacity, altitude, spacing, cleanup, radius, overlap | `UGP_OrbitalDeliverySettings` |
| Manifest fields | still `WorkerCount` / `SalvageWalkerCount` (no UI rewrite) |

`ComputeManifestCosts` and payload spawn resolve from the **one canonical** unit drop definition per type.

Authored production path (GP-S39E correction):

1. Settings soft refs `WorkerDropDefinition` / `SalvageWalkerDropDefinition` (and building slot refs) select the DataAsset. They hold no balance values.
2. Loaded authored definition is canonical for cost, slots, payload, UnitDefinition link, descent, and deploy.
3. Empty ref → native bootstrap immediately.
4. Valid unloaded ref → `RequestAsyncLoad`. Pending orders reject `DefinitionNotReady` (no spend / reserve / pod).
5. Load failure → log + native bootstrap.
6. Deprecated settings numerics/class apply only when the resolved definition cannot provide a valid value. Empty authored `PayloadClass` may still use the operator BP payload bridge.

Building acquisition has the same class of seam: `LogisticsHubDropDefinition` / turret / wallturret soft refs. Wall Package uses a **separate** `UGP_WallPackageDefinition` soft ref (not READY `DA_GP_OrbitalDrop_Wall`). Native catalog must not permanently shadow an assigned authored drop.

## Order UI (target / TEMP)

- **Unit panel:** manifest builder — slots used/cap, counts, per-unit costs, total Orbital, Confirm.
- **Building panel:** Purchase buttons; READY list; click READY → ghost deploy mode.
- **Wall:** Buy Wall Package when stock==0 and not in-flight; Build Wall when stock>0. Package never enters READY/ghost.
- TEMP HUD buttons acceptable for first unit slice; production Order Menu later.

## RPCs (illustrative)

| RPC | Purpose |
| --- | --- |
| `Server_RequestUnitDrop(Manifest)` | Confirm packed unit order |
| `Server_RequestBuildingPurchase(DropDef)` | Spend → READY++ |
| `Server_RequestBuildingDeploy(DropDef, Loc, Rot)` | Consume READY → pod |
| `Server_RequestWallPackage(PackageDef)` | Spend → one rocket to MainBase; pending until arrival |

Client intent only. Rejection via existing notify path where possible.

## Replication / presentation

| State | Cond |
| --- | --- |
| DropPod LandingLocation, TeamId, DescentProgress01 | COND_None (telegraph) |
| OrbitalFerronite | COND_OwnerOnly |
| Building READY inventory | OwnerOnly (or equivalent) |
| MainBase Wall segment count + package pending | Owner-readable |
| Mesh/Niagara | Authored BP; not gameplay-hardcoded |

## Cosmetic budget (MVP)

Shared rocket: vertical descent, exhaust while moving, impact smoke, clear, payload appear. No unique unit vs building animation required in MVP.

## Pillar 8

1–2 sentences: “Ship to orbit, spend Orbital Ferronite; pack units onto a base pad rocket, buy buildings into READY inventory and place them later, or land a 5-segment Wall Package at MainBase and Build Wall from stock.”

## Acceptance sketches

| # | Case |
| --- | --- |
| 1 | Unit manifest Confirm → spend once → pod → Drop Zone → units controllable |
| 2 | Insufficient Orbital → reject, no spend |
| 3 | Slots overflow → reject |
| 4 | Unit cap overflow → reject full manifest |
| 5 | Building Purchase → Orbital↓, READY++ |
| 6 | Deploy → READY--, no second spend, building lands |
| 7 | Esc cancel deploy → READY unchanged |
| 8 | Duplicate deploy RPC → no double spawn / no double consume |
| 9 | Multi-unit offsets non-overlapping |
| 10 | Wall Package purchase → spend once → rocket to MainBase → stock 5; no placement |
| 11 | Wall Package while stock>0 or in-flight → reject |
| 12 | Build Wall is not an orbital purchase (inventory consume in GP-S42C) |

## Out of this TDD’s first impl slice

Full FoW, production Order Menu polish — later. Wall Package + inventory = **GP-S42A**; drag placement = **GP-S42C**. GP-S36G added BuildGrid occupancy + snap. GP-S37T added deployable `AGP_DefensiveTurret` (yaw-0 rectangular reservation, native 2×2).

## References

- GDD/10, GDD/02, GDD/04, GDD/05
- ADR-0009 (+ 2026-08-08 refinement)
- FoW TDD/15, Build Grid TDD/06 (building deploy)
- UI TDD/12
