# GP — FoW Client Presentation Foundation

**Status:** `FOW_CLIENT_PRESENTATION_FOUNDATION_FINALIZED_READY_FOR_MERGE`
**Branch:** `feature/gp-fow-client-presentation-foundation`
**Base:** `origin/main` @ `9d9caa5fc7665ad8dc62016aed7b77f5238761dc`

## Goal

Provide a trusted, presentation-only owning-client mirror of authoritative FoW and the first production
GPUIRuntime CommonUI/MVVM projection. Visual fog, minimap, the full HUD, last-known state, and relevance
filtering are explicitly outside this slice.

## Factual pre-change state

- `GPUIRuntime` contained only its module shell; no project ViewModel, adapter, or CommonUI widget base.
- CommonUI and ModelViewViewModel were already enabled in `GP.uproject`; `GPUIRuntime.Build.cs` already
  depended on `CommonUI`, `CommonInput`, `ModelViewViewModel`, `GPRuntime`, and `GPGASRuntime`.
- `UGP_FogOfWarComponent` was GameState-owned and authority-only; raw per-team bits were not replicated.
- `AGP_PlayerController` already owned local Selection/Command subobjects, PlayerState lifecycle hooks,
  client RPC conventions, placement preview, and the unchanged TEMP HUD.
- `AGP_PlayerState::TeamId` replicated with `OnRep_TeamId` and a native change delegate.

## Implemented network/presentation architecture

- `UGP_LocalFoWComponent` is a non-replicated default subobject of `AGP_PlayerController`.
- It stores exactly one local team: metadata, Explored, Visible, readiness, and revision.
- It has no TeamId query parameter and no map of team grids.
- `AGP_PlayerController` is the sync owner. Authority binds to the existing FoW recompute change event
  and sends one reliable owning-client RPC; there is no client-to-server FoW RPC.
- Initial update: full Explored + current Visible compressed ranges, grid metadata, team, revision.
- Ongoing update: newly Explored ranges + compact complete current Visible ranges + revision.
- Full current Visible replacement makes removal deterministic; Explored only accumulates.
- Stale/duplicate revisions, team mismatches, metadata mismatches, and invalid ranges are rejected before
  mutation.
- PlayerState replacement, team change, EndPlay, and seamless travel clear prior-team presentation data.
- Standalone/listen host uses the same Client RPC implementation as a remote owning client.

## Bandwidth model

No raw one-million-cell bitmap is replicated. Row-major contiguous ranges compress the current circle
coverage, initial sync sends full state once, and ongoing updates send only new exploration plus current
visibility when authoritative state actually changes. There is no per-frame RPC or local recompute.

The 1000 x 1000 grid is a hard one-million-cell bound. Each run is two `int32` values (8 bytes before
UE RPC serialization overhead). Circle visibility normally produces at most one run per intersected row
per isolated source (`2 * ceil(Radius / CellSize) + 1` before overlap merging). There is no separate
byte cap/chunking layer; the theoretical alternating-cell worst case is 500,000 runs (~4 MB before
serialization), so very large/fragmented late-match reconnect snapshots remain a future scalability
concern rather than a claim of unlimited payload safety.

## GPUIRuntime foundation

- `UGP_FoWViewModel : UMVVMViewModelBase` exposes FieldNotify metadata, readiness, and coarse revision.
- Future native presentation can pull state by world location after a revision notification; no giant
  per-cell FieldNotify graph exists.
- `UGP_FoWViewModelAdapter` binds to the mirror's push delegate, owns the ViewModel, never ticks, and
  never scans the world.
- `UGP_ActivatableWidgetBase : UCommonActivatableWidget` establishes the smallest project-owned CommonUI
  modal base required by TDD/12.
- Dependency remains `GPUIRuntime -> GPRuntime -> GPGASRuntime`; GPRuntime does not depend on GPUIRuntime.
- No authored Widget Blueprint or screen stack was created.

## Consumers and boundaries

- Local building placement preview now conservatively reports `NotVisible` when the mirror is not ready
  or the snapped point is not Visible. Server confirmation remains independently authoritative.
- Selection/inspect receives a safe local query surface but is not integrated in this slice.
- `UGP_TEMP_S28P_PlanetaryFerroniteHUD` is unchanged and remains the active operator surface.
- Deferred: visual world fog, terrain presentation, minimap, last-known snapshots, relevance hiding,
  full selection/inspect integration, explicit-Attack last-known behavior, full HUD/Order Menu/
  notifications/end screen, and DropPod temporary vision.

## Validation

- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- `gp.Resource.RunPlanetaryFerroniteHUDContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS**
- GP Win64 Shipping — **PASS**

The focused contract proves mirror readiness, metadata, visible/explored transitions, monotonic
exploration, stale/duplicate rejection, atomic invalid-range rejection, reset/reconnect, two owning-team
isolation at one coordinate, ViewModel push refresh, MVVM/CommonUI bases, and absence of arbitrary-team
or raw-mirror replication APIs.

The existing harness does not create two real owning network connections for this component-level
contract. Live listen-server/two-player ownership and restart/reinitialization were therefore validated
in operator PIE rather than faked with two team arrays in one mirror.

## Operator validation — PASS

- Listen-server `LocalDump`: Ready, Team 1, advancing revision, 200 cm cells, origin
  `(-100000,-100000)`, dimensions `1000 x 1000`.
- `LocalQueryState -1790 -2620`: Visible.
- Point approximately `(-1130,1637)`: Visible at revision 105; after the LongRange sight source moved
  away, Explored at revision 172.
- Placement preview: non-Visible terrain locally blocked as Not Visible; Visible terrain remained
  placeable; server confirmation remained functional.
- Two-player PIE: remote client reported `NetMode=Client`, Team 2, Ready, revision 3; host reported
  `NetMode=ListenServer`, Team 1, Ready, revision 13.
- Same coordinate `(1800,-670)`: Team 2 remote client Visible; Team 1 listen host Unexplored.
- Repeated PIE restart/recreation consistently returned a Ready mirror with the correct local team.
- No Widget Blueprint or authored content edit was required.

## Final state

The trusted local mirror, server-originated sync, revision/team isolation, MVVM/CommonUI foundation, and
local FoW-aware placement preview are finalized. Visual fog, minimap, full HUD, selection/inspect
presentation, last-known/relevance, DropPod sight, SWARM, and building redesign remain separate work.

**NOT MERGED.**
