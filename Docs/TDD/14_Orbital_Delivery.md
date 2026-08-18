# Orbital Delivery System

## Scope

Engineering implementation of orbital drop pods (per [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md)). Defines dual procurement flows (units vs buildings), shared DropPod actor, validation, replication, presentation hooks, subsystem ownership. Replaces pre-pivot Production/Construction/GhostBuilding.

> **Owner refinement (2026-08-08):** See GDD/10. Units → MainBase Unit Drop Zone + transport-slot manifests. Buildings → Purchase/READY inventory then Deploy (no second spend). Shared rocket visual family; authored BP for mesh/Niagara.

## Hard Rules

1. **All non-initial assets arrive from orbit.** No local production / construction.
2. **Server-authoritative.** Client intent only; server validates and schedules pods.
3. **Two flows, one delivery actor.** Unit manifests and building deploys share `AGP_DropPod` + presentation BP; differ in targeting + payload data.
4. **Units do not free-place.** Landing = server-resolved MainBase **Unit Drop Zone** (authored anchor — not hardcoded `BaseLocation + offset`).
5. **Buildings: spend on Purchase, not on Deploy.** Deploy consumes READY inventory once.
6. **Telegraph 2–3 s** (data-driven). Visible to all clients.
7. **No client prediction** of spend / inventory / payload.
8. **DataAsset-driven** costs, slot costs, pod capacity, descent duration, soft payload/visual refs.
9. **GAS spend only** via Instant spend GE (no direct attribute mutate). Spend exactly once on accepted unit Confirm / building Purchase.
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

Owns enqueue for both flows, active pods, validation helpers, READY building inventory mutation (or delegates to a minimal owner-only replicated component on PlayerState — pick smallest server-authoritative structure at impl).

Conceptual APIs (names illustrative):

- `TryEnqueueUnitManifest(Requester, Manifest, OutReason)`
- `TryPurchaseBuilding(Requester, BuildingDropDef, OutReason)`
- `TryDeployBuilding(Requester, BuildingDropDef, Loc, Rot, OutReason)`

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

Single-payload DropDef for buildings; multi-payload carried as **manifest on the order**, not N separate pods unless future design says otherwise. MVP: **one pod per confirmed unit order**; **one pod per building deploy**.

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
5. **GP-S33C / GP-S35B:** Logistics Hub `UGP_GE_UnitCap_Plus5` applies when the **payload building is live/operational**, not at Purchase READY, ghost, or while the DropPod is descending. Native Hub actor logic — not DA `EffectsOnPlacement`. Editor-placed owned live Hubs also grant the bonus once.

## Order UI (target / TEMP)

- **Unit panel:** manifest builder — slots used/cap, counts, per-unit costs, total Orbital, Confirm.
- **Building panel:** Purchase buttons; READY list; click READY → ghost deploy mode.
- TEMP HUD buttons acceptable for first unit slice; production Order Menu later.

## RPCs (illustrative)

| RPC | Purpose |
| --- | --- |
| `Server_RequestUnitDrop(Manifest)` | Confirm packed unit order |
| `Server_RequestBuildingPurchase(DropDef)` | Spend → READY++ |
| `Server_RequestBuildingDeploy(DropDef, Loc, Rot)` | Consume READY → pod |

Client intent only. Rejection via existing notify path where possible.

## Replication / presentation

| State | Cond |
| --- | --- |
| DropPod LandingLocation, TeamId, DescentProgress01 | COND_None (telegraph) |
| OrbitalFerronite | COND_OwnerOnly |
| Building READY inventory | OwnerOnly (or equivalent) |
| Mesh/Niagara | Authored BP; not gameplay-hardcoded |

## Cosmetic budget (MVP)

Shared rocket: vertical descent, exhaust while moving, impact smoke, clear, payload appear. No unique unit vs building animation required in MVP.

## Pillar 8

1–2 sentences: “Ship to orbit, spend Orbital Ferronite; pack units onto a base pad rocket, or buy buildings into READY inventory and place them later.”

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

## Out of this TDD’s first impl slice

Full FoW, Wall gameplay, production Order Menu polish — layered on the same DropPod pipeline in later slices. GP-S36G added BuildGrid occupancy + snap. GP-S37T added deployable `AGP_DefensiveTurret` (yaw-0 rectangular reservation, native 2×2).

## References

- GDD/10, GDD/02, GDD/04, GDD/05
- ADR-0009 (+ 2026-08-08 refinement)
- FoW TDD/15, Build Grid TDD/06 (building deploy)
- UI TDD/12
