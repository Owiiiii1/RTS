# GP — Fog of War World Visualization

**Status:** `FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION`
**Branch:** `feature/gp-fow-world-visualization`
**Base:** `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Goal

Make the trusted one-team local FoW physically visible over the current MVP arena:

- Unexplored: opaque black;
- Explored: dim/grey terrain;
- Visible: unchanged SceneColor.

This slice is presentation-only. Authority, sight computation, replication relevance, last-known state,
selection/inspect policy, minimap, and production HUD remain separate.

Gameplay FoW remains **200 cm / 5 Hz**. A 50 cm / 10 Hz grid change was considered and **aborted
before implementation**. Visual smoothness now comes from a world-space post-process texture mask
with spatial blur and temporal interpolation, not from a finer gameplay grid.

## Pre-change rendering audit

- No project-owned FoW material, post-process material, render target, runtime texture, procedural FoW
  mesh, world overlay, or screen-space FoW renderer existed under production source/tracked
  `/Game/GrimProtocol` before this visualization slice.
- `AGP_CameraPawn` is perspective: spring-arm zoom 1200–4500 cm, pitch interpolates from -45 to -65
  degrees, unrestricted yaw rotation, camera-relative pan, and viewport edge scrolling.
- Read-only runtime audit found zero `Landscape` objects in `L_PrototypeArena`; current gameplay ground
  is the planar blockout around Z=0.

## Selected architecture

`UGP_FoWWorldPresentationSubsystem` remains the single local-player owner in `GPUIRuntime`.

On LocalFoW revision it builds a **1024×1024** Known/Visible runtime texture covering the current
200000×200000 cm FoW bounds (~195 cm / visual texel). A second texture holds the previous visual
target. A per-view `FSceneViewExtension` injects a dynamic instance of
`/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess` into `FinalPostProcessSettings` for that
LocalPlayer viewport only.

- R = KnownMask (Explored OR Visible)
- G = VisibleMask (currently Visible)
- Material reconstructs world XY, maps to FoW UV, lerps Previous→Target by `FoWBlendAlpha`, and
  darkens SceneColor: Unexplored black, Explored dim, Visible unchanged.
- Spatial filter: 2-pass separable box, radius 1 texel (~195 cm).
- Temporal blend: 0.20 s after each LocalFoW revision. Gameplay queries stay exact and immediate.
- NotReady forces `FoWReady=0` (full black). No map PostProcessVolume is required.
- Camera pan/zoom/yaw does **not** rebuild the mask.

## Rejected alternatives

- Post-process was previously deferred because no material existed; this slice adds one isolated
  authored FoW material under `/Game/GrimProtocol/FogOfWar/`.
- Material Parameter Collection: rejected; global world state could leak one local team's mask.
- Full one-million-cell runtime texture: unnecessary; 1024² is presentation-only.
- Source-only Slate projected runs, 0.22-cell feather, dual marching squares, SDF+Chaikin, and the
  viewport-local 10× presentation raster: operator-rejected (staircase, square silhouette, excessive
  blur, camera full-black, left-side striping, too much custom geometry). Deleted.

## Visual and performance contract

- Gameplay CellSize stays 200 cm; authority recompute stays 0.20 s / 5 Hz; LocalFoW protocol unchanged.
- Presentation texture is independent of gameplay cell count.
- Unexplored composition: black. Explored: SceneColor × 0.35. Visible: unchanged SceneColor.
- Spatial smoothing and temporal interpolation do not mutate LocalFoW or authority.
- Two mask textures per LocalPlayer (Previous + Target), RGBA8, ~4.0 MB each.
- CPU mask rebuild and GPU upload run only on LocalFoW revision, not per camera frame.
- Enemy unit/building presentation and health-bar composition remain a separate world-presentation
  gate (`UGP_LocalFoWUnitPresentationSubsystem` / `UGP_HealthBarComponent`).

## Lifecycle and multiplayer

- `ULocalPlayerSubsystem` + scene-view extension follow the local PlayerController/viewport.
- Mirror NotReady/reset remains conservatively black.
- New ready snapshots and newer deltas rebuild the **target** mask and reset blend alpha.
- Every local player owns separate textures and a unique MID; no global mask or MPC is used.
- `gp.FoW.VisualEnable 0/1` changes presentation only.

## Enemy presentation / health-bar composition

Unchanged from the validated correction:

- own-team and neutral presentation stays allowed;
- cross-team unit/building presentation is allowed only at a LocalFoW `Visible` location;
- health bars remain `owner/death gate && FoW presentation gate && damaged-health policy`.

This does not implement `IsNetRelevantFor` or last-known snapshots.

## Diagnostics

`gp.FoW.VisualDump` reports Renderer, MaskModel, TextureResolution, WorldOrigin, WorldExtent,
MaskRevision, PreviousRevision, BlendAlpha, BlendDuration, SpatialFilter, SpatialRadius,
MaskBuildMs, MaskUploadMs, LocalTeam, Ready, PostProcessBound, OldSlateRendererActive=false.

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

No PlayerController, building-placement, TEMP HUD, gameplay authority, or replication policy changed
beyond the presentation renderer. Gameplay FoW remains 200 cm / 5 Hz.

## Operator test

1. Start normal Team 1 PIE.
2. Confirm starting sight regions are normal and untouched territory is black. No full-map flash.
3. Move a Worker or LongRange unit outward; black becomes Visible; leaving a cell becomes dim/grey,
   never Unexplored black.
4. Pan, zoom, and yaw, including fast pan past the previous view: the mask must stay world-locked.
   No left-side striping, no permanent full-black, no geometry left behind.
5. Isolated sight should read as a soft region (spatial + temporal), not a hard 200 cm square.
6. Damage an own unit/building: bar appears; heal to full: bar hides; death/zero remains hidden.
7. In two-player listen-server PIE, move a damaged enemy between Visible and hidden cells. Confirm
   mesh/team/bar hide in Explored/Unexplored and restore in Visible. Team 1 and Team 2 must not share
   a mask.
8. `gp.FoW.VisualDump` should show `Renderer=PostProcessTextureMask`, `MaskModel=Known+Visible`,
   `TextureResolution=1024`, `OldSlateRendererActive=false`, `PostProcessBound=true`, `CellSize=200`.
9. Toggle `gp.FoW.VisualEnable 0`, then `1`, for A/B confirmation.

## Deferred

- gameplay FoW 50 cm / 10 Hz (aborted; evaluate this renderer first)
- enemy actor replication relevance (`IsNetRelevantFor`) and last-known representation
- hidden selection/inspect policy
- minimap/minimap FoW
- full production HUD, Order Menu, notifications, end screen
- DropPod temporary sight

The only new authored asset is `/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess`.
Existing maps, Blueprints, DataAssets, materials, VFX, Config, and Tools were not modified.

**NOT MERGED. NOT FINALIZED.**
