# Cursor Work Report — FoW Client Presentation Foundation Finalization

## Status

**FOW_CLIENT_PRESENTATION_FOUNDATION_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-fow-client-presentation-foundation`
- Exact base: `9d9caa5fc7665ad8dc62016aed7b77f5238761dc`
- Validated implementation head before finalization docs:
  `30edb117bda035172646c83e6235e5c60817b748`
- Finalization documentation/report head: `c248f2c46a2f010e388dd5eb9a18c66c415fb702`
- Final branch head: documentation-only head-record commit following the finalization report

## Operator validation — PASS

Single-player/listen-server local mirror:

- `gp.FoW.LocalDump`: `Ready=true`, `LocalTeam=1`, advancing revision
- `CellSize=200`
- `Origin=(-100000,-100000)`
- `Dims=1000x1000`
- `gp.FoW.LocalQueryState -1790 -2620` -> `Visible`

Visible removal and persistent exploration:

- point approximately `(-1130,1637)` was `Visible` at revision 105
- after the LongRange Salvage Walker moved beyond its FoW radius, the same point was `Explored` at
  revision 172
- this proves current Visible replacement/removal and monotonic Explored on the local mirror

Building placement:

- non-Visible terrain locally reported blocked / Not Visible
- Visible terrain remained placeable
- independent server authority confirmation remained functional

Two-player listen-server PIE:

- remote client: `NetMode=Client`, `Ready=true`, `LocalTeam=2`, revision 3
- listen host: `NetMode=ListenServer`, `Ready=true`, `LocalTeam=1`, revision 13
- same coordinate `(1800,-670)`: Team 2 remote client `Visible`; Team 1 listen host `Unexplored`
- this directly proves Team 1 did not receive Team 2 current visibility

Restart/reinitialization:

- repeated PIE restart/recreation consistently returned a Ready mirror with the correct local team
- reconnect/reinitialization operator gate: **PASS**

## Final authority audit

- `UGP_FogOfWarComponent` remains the GameState-owned server authority.
- Authority state remains non-replicated and uses per-team `TBitArray` internally.
- `UGP_LocalFoWComponent` is presentation-only; no gameplay authority consumes it.
- No client-to-server FoW mutation RPC exists.
- Server building placement validation still reads authoritative FoW independently.
- Local placement preview cannot bypass server validation.
- Auto-acquire and all other server gameplay consumers remain on authoritative FoW.

## Final client mirror architecture

- `UGP_LocalFoWComponent` is a non-replicated PlayerController default subobject.
- It stores exactly one local team: grid metadata, Explored, Visible, readiness, and revision.
- It has no map of all teams and no arbitrary TeamId query API.
- Initial snapshot for a valid owning team contains metadata, complete Explored, complete current
  Visible, and authoritative revision.
- Ongoing updates contain newly Explored plus complete current Visible and a monotonically increasing
  revision.
- Visible replacement supports additions/removals; removed Visible remains Explored.
- Explored only accumulates.
- Invalid metadata/ranges reject atomically.
- Stale/duplicate revisions, team mismatches, and grid mismatches reject before mutation.
- Listen host and remote client use the same server-originated owning-client RPC flow.

## Sync ownership / PlayerController lifecycle

`AGP_PlayerController` is the only sync/publishing owner:

- binds the authoritative FoW change delegate on authority
- sends one owning-client reliable RPC when that team revision changes
- sends a complete initial snapshot for a new/reinitialized owning controller
- handles BeginPlay, PlayerState readiness/replacement, TeamId changes, BeginPlayingState,
  seamless travel, and EndPlay reset/unbind
- contains no second FoW timer or polling loop

`UGP_LocalFoWComponent` and its ViewModel adapter never tick. The authoritative 0.20 s recompute is the
only periodic FoW loop.

## Bandwidth model

- No raw `TBitArray` is replicated.
- No one-million-cell bitmap is sent every update.
- Row-major contiguous runs are encoded as `{StartIndex, NumCells}`.
- Initial state is sent once; ongoing state is sent only on authoritative team revision changes,
  normally at the 5 Hz recompute cadence.
- Explored sends additions after initial sync; Visible sends a compact current replacement.
- The grid is bounded to 1,000,000 cells.
- Each run is two `int32` values: 8 bytes before UE RPC serialization overhead.
- Circular vision normally contributes at most one run per intersected row per isolated source:
  `2 * ceil(Radius / CellSize) + 1` before overlap merging.
- No hard chunk/byte cap exists. The theoretical alternating-cell worst case is 500,000 runs
  (~4 MB before serialization), so highly fragmented large reconnect snapshots remain a future
  scalability concern outside the operator-validated MVP match scale.
- There is no per-frame FoW RPC.

## Team isolation guarantees

- Server extraction uses only the owning PlayerState TeamId.
- The payload carries one TeamId; the mirror stores one team.
- Team/PlayerState/travel reset clears prior-team bits.
- A different team requires a complete initial snapshot.
- No local API can request another TeamId.
- Contract isolation and real same-coordinate two-player operator isolation both passed.

## GPUIRuntime / MVVM audit

- Dependency remains `GPUIRuntime -> GPRuntime -> GPGASRuntime`.
- GPRuntime has no dependency on GPUIRuntime.
- `UGP_FoWViewModel : UMVVMViewModelBase` remains read-only presentation state.
- `UGP_FoWViewModelAdapter` subscribes to `OnLocalFoWUpdated`; it does not poll or scan the world.
- `Revision` remains the coarse FieldNotify invalidation mechanism for per-cell consumers.
- `UGP_ActivatableWidgetBase : UCommonActivatableWidget` remains the minimal justified project base.
- No router, screen stack, HUD root, or authored Widget Blueprint was added.
- `UGP_TEMP_S28P_PlanetaryFerroniteHUD` lifecycle and behavior remain unchanged; its focused regression
  passed.

## Building placement preview

- Trusted local visibility is presentation-only.
- NotReady and non-Visible both produce local NotVisible preview rejection.
- Visible permits the preview to continue through its other local checks.
- Authority confirmation independently reruns server validation.
- No authored material, ghost asset, Blueprint, or map was changed.

## Completed capability state

Done:

- authoritative three-state per-team FoW runtime
- trusted one-team local mirror
- server-originated initial/delta presentation sync
- stale/invalid revision protection
- team isolation and restart/reinitialization
- FoW MVVM ViewModel/adapter foundation
- project CommonUI activatable base
- local FoW-aware building placement preview

Still remaining:

- visual black/grey world FoW and terrain presentation
- render target/post-process/decal fog
- minimap and minimap FoW
- last-known snapshots / unit blip fading
- replication relevance hiding
- hidden-enemy selection/inspect presentation
- explicit Attack last-known pursuit/re-engage
- full production HUD
- Order Menu
- notifications and end screen
- DropPod temporary sight
- SWARM
- building redesign

## Final contracts / regressions

- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- `gp.Resource.RunPlanetaryFerroniteHUDContractTest` — **PASS**, `Failures=0`

No separate PlayerController/network automation command exists. The focused presentation contract,
TEMP HUD lifecycle regression, and real two-player operator PIE cover the directly affected seams.

## Final builds

- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS**
- GP Win64 Shipping — **PASS**

No target was rebuilt after a successful result.

## Risk / escalation verdict

The factual blast radius is PlayerController owner RPC/lifecycle, FoW snapshot extraction and revision
tracking, local placement preview, and the first GPUIRuntime ViewModel/base. All five focused contracts,
the real two-player operator isolation test, and all three final builds passed.

No failure or shared-framework regression triggered full historical-suite escalation. UnitDefinition,
vitals, movement, combat FSM, and unrelated economy suites were not rerun because those paths did not
change.

## Exact changed files in the slice

Runtime:

- `GP/Source/GPRuntime/Public/FogOfWar/GPFoWPresentationTypes.h`
- `GP/Source/GPRuntime/Public/FogOfWar/GPLocalFoWComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPLocalFoWComponent.cpp`
- `GP/Source/GPRuntime/Public/FogOfWar/GPFogOfWarComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPFogOfWarComponent.cpp`
- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`

UI runtime:

- `GP/Source/GPUIRuntime/Public/ViewModels/GPFoWViewModel.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPFoWViewModel.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPFoWViewModelAdapter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPFoWViewModelAdapter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPActivatableWidgetBase.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPActivatableWidgetBase.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWClientPresentationFoundationContractTest.cpp`

Documentation:

- `Docs/Development/Claude_Tasks/GP-FoW-Client-Presentation-Foundation.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected-content confirmation

The branch diff from base contains no Config, map, Blueprint, DataAsset, material, VFX, Tools, or other
authored Content.

All pre-existing local protected changes remain untouched. In particular, the operator-local LongRange
Salvage Walker UnitDefinition with `Fog Of War Sight Radius = 2000` remains untracked and was not
staged, committed, reverted, stashed, cleaned, restored, or otherwise modified.

## Roadmap after finalization

The authority and trusted-client FoW foundations are done. Visual world Fog of War presentation is the
next bounded production capability. This does not mark minimap, full production HUD, relevance,
last-known behavior, or the complete FoW feature done.

**NOT MERGED.**
