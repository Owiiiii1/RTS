# Cursor Work Report — FoW Client Presentation Foundation

## Status

**FOW_CLIENT_PRESENTATION_FOUNDATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / baseline / head

- Branch: `feature/gp-fow-client-presentation-foundation`
- Exact base: `origin/main` @ `9d9caa5fc7665ad8dc62016aed7b77f5238761dc`
- Validated implementation/report head: `b19d7befbd3177ead332e2e41a4b508cc3e36ca8`
- Final branch head: documentation-only head-record commit following the validated implementation

## Factual pre-change UI architecture

- `GPUIRuntime` existed as an empty module shell with no project ViewModel, adapter, or native CommonUI
  widget base.
- `CommonUI` and `ModelViewViewModel` were already enabled in `GP.uproject`.
- `GPUIRuntime.Build.cs` already declared `CommonUI`, `CommonInput`, `ModelViewViewModel`, `UMG`,
  `GPRuntime`, and `GPGASRuntime`.
- Intended dependency direction was already `GPUIRuntime -> GPRuntime -> GPGASRuntime`; this slice did
  not add a reverse GPRuntime dependency.
- `AGP_PlayerController` already owned local Selection/Command subobjects, lifecycle hooks, client RPC
  conventions, placement preview, and the TEMP HUD.
- `AGP_PlayerState::TeamId` was replicated through `OnRep_TeamId` and a native change delegate.
- No generic owner-only snapshot/delta service existed to reuse.

## Client mirror owner

`UGP_LocalFoWComponent` is a non-replicated default subobject of `AGP_PlayerController`. It represents
exactly one owning player's team and exposes only:

- `GetStateAtWorldLocation`
- `IsExplored`
- `IsVisible`
- readiness, local team, grid metadata, and revision getters

It contains no per-team map and no arbitrary TeamId query surface. It is presentation-only; local
mutation cannot reach authoritative `UGP_FogOfWarComponent` state.

## Sync owner and protocol

`AGP_PlayerController` is the single sync/publishing owner:

- authority binds to `UGP_FogOfWarComponent::OnTeamStateChanged`;
- an owning-client reliable RPC carries server-originated presentation updates;
- there is no client-to-server FoW RPC;
- reconnect/new PlayerController, PlayerState replacement, team change, and seamless travel trigger
  reset/complete initialization behavior;
- LocalFoW itself does not tick or poll.

Initial snapshot:

- local TeamId
- grid origin/dimensions/cell size
- complete current Explored ranges
- complete current Visible ranges
- authoritative per-team revision

Ongoing changed-recompute update:

- newly Explored ranges only
- complete current Visible ranges
- monotonically increasing revision

The complete Visible replacement handles additions/removals without tombstones. Explored only
accumulates.

## Bandwidth / validation model

- Raw `TBitArray` and a one-million-cell bitmap are never replicated.
- Sorted row-major cell runs are compressed as `{StartIndex, NumCells}`.
- Initial state is sent once; ongoing updates are sent only when authoritative team state changes,
  normally at the 0.20 s / 5 Hz recompute cadence.
- Current circle-source topology compresses into row runs; there is no per-frame RPC.
- Invalid metadata/ranges reject the complete update before mutation.
- Stale and duplicate revisions reject safely.
- Delta team or grid mismatch rejects safely.
- Visible always implies Explored in the mirror.

## Team isolation and network roles

- Every PlayerController extracts only its own replicated PlayerState TeamId.
- Team change/reset clears prior-team bits before a different-team snapshot is accepted.
- Listen host and remote owning clients use the same Client RPC implementation.
- Non-owning clients receive no other team's payload through another controller.
- Standalone uses the same path as listen host.

The deterministic contract creates two independent owning-team mirrors and proves different state at
the same coordinate with no cross-team query API. The existing command-contract harness does not create
two real network-owned PlayerControllers; live listen-server/two-player isolation and reconnect remain
the required operator PIE gate rather than a fake in-object multiplayer claim.

## ViewModel architecture

- `UGP_FoWViewModel : UMVVMViewModelBase` is the first production GP ViewModel.
- FieldNotify properties: `LocalTeamId`, `GridOrigin`, `GridDimensions`, `CellSizeCm`, `Revision`,
  and `bIsReady`.
- `Revision` is the coarse notification token for future region-based render/minimap pulls; no giant
  per-cell FieldNotify model exists.
- Read-only `GetStateAtWorldLocation` delegates to the trusted mirror.
- `UGP_FoWViewModelAdapter` creates/owns the ViewModel, binds to the mirror update delegate, refreshes
  push-style, and unbinds on shutdown/destruction.
- The adapter does not tick and does not scan the world.

## CommonUI and TEMP HUD

- Added `UGP_ActivatableWidgetBase : UCommonActivatableWidget` because TDD/12 requires a project-owned
  CommonUI base and the plugins/modules were already configured.
- No HUD root, router, screen stack, or authored Widget Blueprint was added.
- `UGP_TEMP_S28P_PlanetaryFerroniteHUD` was not deleted or rewritten and remains the current
  gameplay/operator surface.

## Gameplay/presentation consumer verdicts

Building placement preview:

- integrated narrowly;
- the local preview reports `NotVisible` if the mirror is not ready or the snapped location is not
  currently Visible;
- server authority still independently validates confirmation;
- no material or ghost asset was edited.

Selection/inspect:

- not integrated;
- future presentation code can consume the safe local query;
- full hidden-enemy selection/inspect and relevance behavior remains a separate slice.

## Explicitly deferred

- black/grey visual world fog and terrain rendering
- post-process/render-target/decal/procedural-mesh work
- minimap and minimap FoW
- last-known snapshots and unit blip fading
- replication relevance hiding
- full selection/inspect FoW integration
- explicit-Attack last-known chase/re-engage
- full production HUD, Order Menu, notifications, and end screen
- temporary DropPod sight
- SWARM and building redesign

## Exact changed files

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

## Contracts / regressions

- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- `gp.Resource.RunPlanetaryFerroniteHUDContractTest` — **PASS**, `Failures=0`

The focused contract proves initial readiness, metadata, local team, Visible/Explored transfer,
Visible-to-Explored removal, monotonic exploration, stale/duplicate rejection, invalid-range atomic
rejection, reset/team replacement, reconnect snapshot, two-owning-team isolation, ViewModel refresh,
CommonUI/MVVM bases, no arbitrary-team query, and no raw mirror replication.

## Multiplayer validation

- Deterministic lower-level two-owner isolation: **PASS**
- Real listen-server/two-player PIE ownership and reconnect: **PENDING OPERATOR VALIDATION**

This is the strongest current automated contract without fabricating network ownership inside the
single-world contract harness.

## Risk / escalation

The change touches PlayerController lifecycle, one owner-only reliable RPC, authoritative FoW extraction,
placement preview, and GPUIRuntime. Risk-based validation therefore included the new contract,
authoritative FoW, building-drop, BuildGrid, and TEMP HUD/controller lifecycle regressions. No shared
gameplay authority semantics changed and no failure triggered full historical-suite escalation.

UnitDefinition/vitals contracts were not rerun because their schema and initialization paths were
untouched.

## Build

- GPEditor Win64 Development + UHT — **PASS**
- GP Development / Shipping — reserved for finalization after operator PASS

## Protected-content confirmation

No Config, prototype map, Blueprint, DataAsset, material, VFX, Tools, or untracked Content file belongs
to this branch diff. Existing local authored changes remain untouched.

In particular, the operator's local LongRange Salvage Walker UnitDefinition FoW sight radius of 2000 cm
remains untracked and is not staged, committed, reverted, stashed, cleaned, or restored.

## Exact operator test

1. Start PIE and run `gp.FoW.LocalDump`.
2. Confirm `Ready=true`, `LocalTeam=1`, and revision updates with visibility changes.
3. Run `gp.FoW.LocalQueryState X Y` at a current Worker; expect `Visible`.
4. Move the Worker away; query the old position and expect `Explored`.
5. Query untouched in-bounds terrain and expect `Unexplored`.
6. Enter building placement: non-Visible terrain must show `BLOCKED: NOT VISIBLE` locally; Visible
   terrain remains placeable and authority confirmation still succeeds.
7. Start two-player listen-server PIE and run `gp.FoW.LocalDump` in each client window.
8. Team 1 must report only `LocalTeam=1`; Team 2 only `LocalTeam=2`.
9. Query a coordinate visible to one team only and verify different states.
10. Reconnect/restart a client and confirm a complete ready snapshot returns.

No Widget Blueprint or authored content edit is required.

**NOT MERGED. NOT FINALIZED.**
