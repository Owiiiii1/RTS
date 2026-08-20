# GP — Fog of War Runtime Foundation

**Status:** `FOW_RUNTIME_FOUNDATION_FINALIZED_READY_FOR_MERGE`
**Branch:** `feature/gp-fow-runtime-foundation`
**Base:** `origin/main` @ `de718725115ddd636b56092bd6197cf0f7a65950`
**Scope:** first production three-state per-team Fog of War slice

## Goal

Establish one server-authoritative query for `Unexplored`, `Explored`, and `Visible`, then prove the
contract through auto-acquire and orbital building placement. Rendering, client mirrors, last-known
snapshots, minimap, CommonUI/MVVM, and replication relevance remain later slices.

## Factual pre-change architecture

- `AGP_GameState` owned replicated match facts and authority registries, but no FoW service.
- `UGP_UnitDefinition::SightRangeCm` tuned combat auto-acquire only; there was no FoW sight owner.
- `UGP_UnitCommandComponent` scanned all `AGP_UnitBase` candidates without FoW filtering.
- `GPBuildingDropAuthority` enforced MainBase radius, BuildGrid, navigation, and environment checks,
  but not active visibility.
- Unit drops target the fixed `AGP_MainBase::UnitDropZone`; they are not free-world placement.
- BuildGrid uses 200 cm cells but has no bounded playable-area owner.

## Implemented authority

- `UGP_FogOfWarComponent` is a non-replicated default subobject of `AGP_GameState`.
- Authority keeps compact per-team `TBitArray` grids internally; raw bits are not replicated.
- Public query API:
  - `GetStateForTeamAtWorldLocation`
  - `IsExploredByTeam`
  - `IsVisibleToTeam`
  - `IsCellVisibleToTeam`
- `Visible` cells are accumulated into `Explored`; only `Visible` is cleared during recompute.
- Invalid/non-playable teams, clients, non-finite locations, and out-of-grid locations fail safely as
  `Unexplored`.
- Team grids are independent; no allied sharing exists.

## Grid and update model

- Cell size: 200 cm.
- No canonical playable-area bounds owner exists. This foundation therefore owns deterministic temporary
  bounds: origin `(-100000, -100000)` and dimensions `1000 x 1000`.
- This grid is separate from BuildGrid occupancy/footprint ownership.
- Authority recomputes at 0.2 seconds (5 Hz).
- Live unit/building sources self-register after async UnitDefinition readiness and unregister on death
  or EndPlay. Team changes trigger immediate refresh.
- Recompute iterates the bounded registry, not an all-world actor scan.
- Coverage is an omnidirectional circle with no LOS, height, cone, stealth, or allied vision.

## Sight ownership and asset safety

- Canonical FoW fields live only on `UGP_UnitDefinition`:
  - `FogOfWarSightRadiusCm`
  - `bGrantsFogOfWarVision`
- Building sight follows the existing
  `UGP_BuildingDefinition -> UnitDefinition -> AGP_UnitBase` ownership bridge. No duplicate
  `UGP_BuildingDefinition` vision fields were added.
- Combat `SightRangeCm` remains separate, so scouting and auto-acquire balance do not become coupled.
- Native bootstrap values:
  - MainBase: 3000 cm
  - Worker: 600 cm
  - Salvage Walker: 900 cm
  - Logistics Hub: 900 cm
  - Defensive Turret: 900 cm
- Empty/load-failed definition compatibility uses matching actor fallback values.
- Read-only Unreal inspection found all five current authored UnitDefinitions inherit non-zero 900 cm
  sight and `bGrantsFogOfWarVision=true`. No authored DataAsset migration is required for this operator
  gate; no `.uasset` was edited.

## Gameplay consumers

### Auto-acquire

`UGP_UnitCommandComponent::FindNearestAutoAcquireTarget` now requires the candidate location to be
`Visible` to the owner team. Existing target eligibility, deterministic priority, range, LOS-at-fire,
damage, retaliation, explicit Attack, and Attack-Move lifecycle are otherwise unchanged.

### Orbital building placement

Authority validation rejects a snapped landing location that is not `Visible` to the requesting team
with `EGP_BuildingDropRejectReason::NotVisible`. Remote-client preview remains optimistic because no
trusted local FoW mirror exists yet; server confirmation is authoritative.

### Unit drops

Unchanged. Current unit drops use the owning MainBase's fixed UnitDropZone and do not expose free-world
landing selection. Temporary in-flight DropPod vision is deferred.

## Explicit boundaries

- Enemy local selection gating is deferred until a trusted local FoW mirror/relevancy path exists.
- Explicit Attack last-known chase/re-engage and hidden-target fire transition are audited but deferred
  as a coherent last-known gameplay integration.
- Broad `IsNetRelevantFor` filtering and persistent last-known static actor state are deferred.
- Fog rendering, terrain masks, minimap layers, blip fading, CommonUI/MVVM, and production notifications
  are deferred.
- No footprint/geometry cleanup or building-system redesign was performed.

## Debug/operator surface

Non-shipping commands:

- `gp.FoW.DebugDump` — logs registered sources, team, location, radius, grant/dead state, and grid config.
- `gp.FoW.QueryState <TeamId> <WorldX> <WorldY>` — logs authoritative state at a location.

## Final validation

- `gp.FoW.RunRuntimeFoundationContractTest` — `Failures=0`
- `gp.Combat.RunAutoAcquireContractTest` — `Failures=0`
- `gp.Combat.RunAttackMoveContractTest` — `Failures=0`
- `gp.Combat.RunRetaliationPursuitContractTest` — `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — `Failures=0`
- `gp.Building.RunBuildGridContractTest` — `Failures=0`
- `gp.Building.RunDefensiveTurretContractTest` — `Failures=0`
- `gp.Units.RunUnitDefinitionContractTest` — `Failures=0`
- `gp.Building.RunBuildingVitalsOwnershipContractTest` — `Failures=0`
- `gp.Resource.RunOrbitalUnitDropContractTest` — `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS**
- GP Win64 Shipping — **PASS**

The shared GameState/UnitBase surface justified a broad affected-contract set. The full historical
resource suite was not run: the selected contracts cover the changed authority, definition, combat,
building-placement, and fixed unit-drop boundaries directly, without introducing the known authored-map
contamination of that suite.

## Operator validation

- **PASS** in cold/open Editor + PIE.
- `gp.FoW.DebugDump`: authority service active, 17 sources, 200 cm cells, 0.20 s updates, and
  independent Team 1 / Team 2 sources.
- Team 1 query at `(-1800, -2300)` returned `Visible`; untouched `(20000, 20000)` returned
  `Unexplored`; moving friendly sight away downgraded the prior cell to `Explored`.
- Hidden enemies were not auto-acquired; visible enemies were auto-acquired normally.
- Authority rejected non-Visible building placement and accepted otherwise-valid Visible placement.
- The authored LongRange definition initially combined 2000 cm attack/combat sight with 900 cm FoW
  sight, so its 900 cm visibility limit was correct. The operator locally changed its FoW sight to
  2000 cm and confirmed expected long-range visibility/fire. That `.uasset` adjustment is not a runtime
  fix and is not committed.

## Final state

The authoritative runtime foundation is finalized and ready for merge. Full Fog of War remains
incomplete: trusted client presentation/mirror, rendering, minimap/UI, last-known behavior, relevance
hiding, selection/inspect presentation gating, and DropPod temporary vision remain future work.

**NOT MERGED.**
