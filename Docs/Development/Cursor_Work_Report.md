# Cursor Work Report — Fog of War World Visualization

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Implementation/report head: commit containing this report

## Factual pre-change rendering audit

- No project-owned FoW post-process material, scene overlay, render target, runtime texture, procedural
  mask mesh, Slate/UMG world FoW overlay, or screen-space projection renderer existed.
- No tracked FoW/post-process/render-target asset exists under `/Game/GrimProtocol`.
- `UGP_MarqueeSelectionWidget` provided a source-only native Slate painting precedent, not a reusable
  world FoW renderer.
- Camera is perspective (`UCameraComponent` default projection), with spring-arm zoom 1200–4500 cm,
  -45 to -65 degree pitch interpolation, yaw rotation, camera-relative pan, and viewport edge scrolling.
- Read-only `obj list class=Landscape` on `L_PrototypeArena` returned zero objects. Current gameplay
  ground is an effectively planar primitive blockout around Z=0; no meaningful terrain elevation was
  found that invalidates flat XY projection for this MVP map.
- Source-only rendering was viable.

## Selected rendering architecture

One `UGP_FoWWorldPresentationSubsystem` per `ULocalPlayer` in `GPUIRuntime` is the visual owner.
It creates one native `UGP_FoWWorldOverlayWidget` at low player-screen Z order.

The widget:

- deprojects the current perspective view to planar Z=0;
- computes a clamped view-local FoW cell rectangle;
- samples only that region from the bound `UGP_LocalFoWComponent`;
- coalesces horizontally contiguous equal non-Visible states;
- projects each run into one Slate quad;
- caches geometry until mirror render serial or camera/view projection changes.

The method supports pan, zoom, yaw rotation, viewport resize, listen host, remote client, and
per-LocalPlayer layers without a global render mask.

## Rejected alternatives

- Post-process material/runtime texture: no reusable material exists; requires a new authored material
  or custom depth-reconstruction shader and a larger rendering architecture.
- Material Parameter Collection: global world state risks one team's mask leaking into another local
  view.
- Full one-million-cell texture: needs a material consumer and large upload policy even though the
  camera sees a small bounded region.
- World procedural/decal mesh: prone to z-fighting, weak at obscuring tall actors, and less robust under
  perspective projection.
- Per-cell widget/component/UObject: prohibited million-object/draw-call model.

## Data source / authority

- Sole data source: owning controller's trusted `UGP_LocalFoWComponent`.
- No server `UGP_FogOfWarComponent` query.
- No unit/actor scan, TeamId inference, sight-circle recomputation, or duplicate gameplay grid.
- No gameplay mutation/RPC.
- Authority runtime and client mirror protocol are unchanged.

## Update model and lifecycle

- DATA: `OnLocalFoWUpdated` increments a render serial and invalidates cached runs.
- VIEW: the volatile overlay compares view-projection matrix, view rect, and local size each paint;
  only a changed view rebuilds geometry.
- Static camera + unchanged mirror reuses cached vertices.
- LocalPlayer subsystem follows PlayerController replacement and recreates its owning-player widget.
- Mirror reset/travel/team change invalidates to conservative black.
- New ready snapshot/delta rebuilds on the next paint.
- PIE restart recreates LocalPlayer subsystem/widget through standard engine lifecycle.

## Visual mapping

- Unexplored: black, obscuration `1.0`.
- Explored: neutral near-black tint, obscuration `0.68`; terrain remains readable but clearly dim.
- Visible: obscuration `0.0`; no overlay geometry.
- Hard authoritative 200 cm cell boundaries; no cosmetic expansion of gameplay visibility.
- Transitions are mirror revision-driven: Unexplored -> Visible, Visible -> Explored, Explored ->
  Visible.
- Explored monotonicity remains owned/proven by LocalFoW.

## NotReady / failure behavior

NotReady, missing mirror, invalid projection, view with no valid ground intersection, or sample budget
overflow renders full-screen opaque black. This prevents a startup/full-reveal leak.

`gp.FoW.VisualEnable 0` intentionally disables presentation for non-shipping A/B diagnostics only and
does not mutate mirror or authority state.

## Grid / memory / performance

- Grid: origin `(-100000,-100000)`, 1000 x 1000, 200 cm, 1,000,000 cells.
- Existing mirror storage: two bit arrays, approximately 250 KB total.
- Renderer allocates no second full-grid representation.
- Maximum sampled view cells: 65,536.
- Maximum coalesced quads per Slate batch: 8,000.
- Worst sampled checkerboard remains bounded to at most 65,536 runs and nine batches.
- Typical current prototype offscreen observation: 1,248 sampled cells, 26 runs, one batch.
- Typical cached geometry in that observation: 104 vertices and 156 indices (only a few KB).
- Camera motion rebuilds bounded view geometry; no whole-grid per-frame polling/upload.

## Multiplayer isolation

- Each LocalPlayer owns a distinct subsystem, widget, and one-team LocalFoW binding.
- No static/global mask and no Material Parameter Collection.
- Listen host and remote client therefore project only their own trusted mirror.
- Two-player visual difference remains an operator PIE gate; lower-level different-mirror state and
  renderer isolation are contract-covered.

## Authored assets

No new authored asset was required or created.

No existing map, Blueprint, DataAsset, material, VFX, Config, Tools, or other Content file was modified
or staged by this slice.

## Diagnostics

- `gp.FoW.VisualDump`
- `gp.FoW.VisualEnable 0/1`

Final render-offscreen dump:

- `Active=true`
- `Ready=true`
- `LocalTeam=1`
- method `ViewportLocalProjectedSlateRuns`
- `Dims=1000x1000`, `CellSize=200`
- 1,248 sampled cells, 26 runs, one batch
- `Dirty=false`

## Known limitation

This is world/terrain pixel obscuration, not actor networking policy. Hidden enemy actors may still
replicate and relevance/selection behavior is not solved. Opaque Unexplored overlay naturally covers
scene pixels, but actor relevance, last-known snapshots, and hidden selection remain separate
capabilities.

Planar Z=0 projection is factual for the current arena. A future meaningfully elevated terrain map
would require a depth-aware projection upgrade without changing gameplay FoW state.

## Explicitly deferred

- minimap / minimap FoW
- `IsNetRelevantFor` actor hiding
- last-known building/deposit snapshots
- dynamic-unit five-second marker
- hidden enemy selection/inspect policy
- explicit Attack last-known pursuit
- DropPod temporary sight
- full production HUD, Order Menu, notifications, end screen
- AI Opponent, SWARM, building redesign

## Exact changed files

Production:

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`

Documentation:

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- Render-offscreen lifecycle/paint/diagnostic smoke — **PASS**
- GPEditor Win64 Development + UHT — **PASS**

The focused contract proves conservative NotReady, exact ordered state mapping, revision transitions,
sticky Explored, stale rejection, reset/new snapshot, one-team source, no authority mutation,
no per-cell object model, bounded million-grid behavior, view-only changes, A/B toggle safety, and
different local masks.

## Risk / escalation

Blast radius is confined to new GPUIRuntime LocalPlayer presentation classes and its contract. No
PlayerController, mirror protocol, authority, placement, TEMP HUD, gameplay, or replication source
changed. The slice contract and both directly coupled FoW foundation regressions passed. No failure or
expanded shared behavior triggered broader-suite, HUD, or building-placement escalation.

GP Development and Shipping are reserved for finalization after operator PASS.

## Protected content

All existing local authored/untracked changes remain untouched. The operator-local LongRange Salvage
Walker UnitDefinition with `Fog Of War Sight Radius = 2000` remains untracked and was not committed,
reverted, stashed, cleaned, restored, or modified.

## Operator test

1. Start normal Team 1 PIE.
2. Confirm only starting sight regions are normal and untouched terrain is black.
3. Move a Worker/LongRange unit outward; black becomes Visible.
4. Move away; old area becomes dim/grey, not black.
5. Move back; dim/grey becomes fully Visible.
6. Pan, zoom, and rotate; verify world alignment.
7. Run `gp.FoW.VisualDump`; confirm active/ready/team/revision and bounded region.
8. Toggle `gp.FoW.VisualEnable 0`, then `1`, for A/B.
9. Run two-player listen-server PIE and confirm Team 1/Team 2 masks differ.

No authored asset editing is required.

**NOT MERGED. NOT FINALIZED.**
