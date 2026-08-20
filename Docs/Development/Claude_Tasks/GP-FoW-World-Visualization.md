# GP — Fog of War World Visualization

**Status:** `FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION`
**Branch:** `feature/gp-fow-world-visualization`
**Base:** `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Goal

Make the trusted one-team local FoW physically visible over the current MVP arena:

- Unexplored: opaque black;
- Explored: dark neutral overlay, terrain still readable;
- Visible: no overlay.

This slice is presentation-only. Authority, sight computation, replication relevance, last-known state,
selection/inspect policy, minimap, and production HUD remain separate.

## Pre-change rendering audit

- No project-owned FoW material, post-process material, render target, runtime texture, procedural FoW
  mesh, world overlay, or screen-space FoW renderer existed under production source/tracked
  `/Game/GrimProtocol`.
- Existing `UGP_MarqueeSelectionWidget::NativePaint` established a source-only Slate draw pattern, but
  no reusable world projection/mask owner existed.
- `AGP_CameraPawn` is perspective: spring-arm zoom 1200–4500 cm, pitch interpolates from -45 to -65
  degrees, unrestricted yaw rotation, camera-relative pan, and viewport edge scrolling.
- Read-only runtime audit found zero `Landscape` objects in `L_PrototypeArena`; current gameplay ground
  is the planar blockout around Z=0. Meaningful terrain elevation requiring depth reconstruction is not
  present in the MVP arena.
- Source-only implementation was viable; no authored material/map/Blueprint asset was required.

## Selected architecture

`UGP_FoWWorldPresentationSubsystem` is the single local-player owner in `GPUIRuntime`.

- It binds only to the owning controller's `UGP_LocalFoWComponent`.
- It creates one native `UGP_FoWWorldOverlayWidget` with a low player-screen Z order.
- Mirror update/reset events increment a render serial. Camera pan/zoom/yaw **resamples** the current
  viewport-local raster; the overlay is not frozen to an old contour cache.
- It deprojects the player view to the Z=0 gameplay plane (four corners plus view center, with a
  skyward-ray fallback so one failed corner cannot force full-black).
- KnownMask (Explored|Visible) and VisibleMask are bilinearly upsampled (~10×, 20 cm texels) and
  separable-box-blurred (8 texels / 160 cm), then composed and drawn as coalesced Slate runs.
- NotReady still falls back to full-screen black. Camera motion no longer permanently loses the mask.

This works for perspective pan, zoom, yaw rotation, window/viewport changes, listen host, remote client,
and split local-player layers without global material state.

## Rejected alternatives

- Post-process material + runtime mask: no reusable material exists; it would require a new authored
  material or custom depth-reconstruction shader and materially more rendering infrastructure.
- Material Parameter Collection: global world state could leak one local team's mask across local views.
- Full one-million-cell runtime texture: requires a material consumer and full/large texture upload
  policy despite the camera viewing only a small region.
- World procedural/decal mesh: can z-fight, does not reliably obscure tall actors, and is less robust
  under the perspective camera.
- Per-cell widgets/components/UObjects: prohibited million-object/draw-call architecture.
- 0.22-cell projected-run edge feather: operator-rejected; it only blurred square corners and left
  the 200 cm cell silhouette intact.
- Dual marching squares (`ConservativeDualMarchingSquares`): operator-rejected; the silhouette still
  read as a 200 cm staircase with a wide blur. Not fixed by more blur, more MS subdivisions, T-only
  tweaks, extra feather quads, or raising gameplay grid resolution.
- Known/Visible SDF + Chaikin (`ConservativeKnownVisibleSDFChaikin`): operator-rejected. The silhouette
  still followed 200 cm steps, and camera motion could drop the overlay into conservative full-black.
  Replaced by a viewport-local bilinear presentation raster with separable blur.

## Visual and performance contract

- Unexplored obscuration: `1.0`, color black.
- Explored obscuration: `0.68`, neutral near-black tint.
- Visible obscuration: `0.0`, no quad.
- Gameplay cells remain 200 cm; no gameplay visibility expansion or state interpolation occurs.
- Presentation bilinearly upsamples Known/Visible cell-center samples (~10× → 20 cm texels) and applies
  a separable box blur (8 texels / 160 cm) so 200 cm blocks are no longer the perceived silhouette.
- Combat, drop, building placement, selection authority, and enemy presentation still read exact
  LocalFoW state. Terrain overlay interpolation is presentation-only.
- Deep Unexplored stays strongly black; Visible interiors stay clear.
- Renderer owns no duplicate gameplay grid and performs no sight-circle computation.
- No full one-million-cell presentation copy is allocated.
- View sampling is capped at 65,536 cells; presentation pixels at 262,144; pad is 2 cells.
- Overlay geometry is capped at 16,384 coalesced quads; interiors use up to 8,000 quads per Slate
  draw batch.
- Camera motion resamples the current viewport-local raster. The last successful overlay is kept if a
  rebuild fails, instead of switching to permanent full-black.
- Existing local mirror storage remains two one-million-bit arrays (~250 KB total).

## Lifecycle and multiplayer

- `ULocalPlayerSubsystem` follows local PlayerController replacement automatically.
- Mirror NotReady/reset remains conservatively black.
- New ready snapshots and newer deltas invalidate the renderer via `OnLocalFoWUpdated`.
- Every local player owns a separate subsystem/widget/mirror; no global mask or MPC is used.
- `gp.FoW.VisualEnable 0/1` changes presentation only.
- Seamless travel and PIE recreation are handled through LocalPlayer/PlayerController and mirror reset
  lifecycles.

## Enemy presentation correction

Operator two-player PIE found that replicated enemy mesh/team/health presentation could remain visible
through black Unexplored terrain.

`UGP_LocalFoWUnitPresentationSubsystem` now owns a presentation-only registry in each game world:

- `AGP_UnitBase` registers/unregisters through BeginPlay/EndPlay; there is no world actor discovery.
- LocalFoW update/reset events evaluate registered actors immediately.
- A bounded 10 Hz registered-list pass catches replicated enemy movement across a static FoW edge
  when no FoW grid revision changes.
- own-team and neutral presentation stays allowed;
- cross-team unit/building presentation is allowed only at a LocalFoW `Visible` location;
- the gate uses actor `SetActorHiddenInGame` for all primitive/authored visuals, explicitly composes
  with the health bar, suppresses local combat debug presentation, and refreshes team tint on restore.

This does not destroy actors, disable collision, alter combat, mutate TeamId/transform, change
replication, or implement `IsNetRelevantFor`. Hidden actors still replicate; relevance and last-known
snapshots remain dedicated later slices.

## Health-bar composition

`UGP_HealthBarComponent` remains attribute-delegate driven:

- full (`Health >= MaxHealth` within `max(KINDA_SMALL_NUMBER, abs(MaxHealth) * 1e-4)`) — hidden;
- damaged and alive (`0 < Health < MaxHealth - tolerance`) — health-policy visible;
- zero/dead — hidden.

Final component visibility is `owner/death gate && FoW presentation gate && damaged-health policy`.
A health update therefore cannot re-show a damaged enemy while LocalFoW presentation denies it.

## Diagnostics

- `gp.FoW.VisualDump`
- `gp.FoW.VisualEnable 0/1`

`VisualDump` reports Algorithm, GameplayCellSize, PresentationSupersample, PresentationTexelWorldSize,
SampledGameplayCells, RasterDims, RasterPixels, BlurRadiusSamples, BlurRadiusCm, MaskRevision,
CameraResample, FallbackActive, RegisteredEnemyPresentation, LocalTeam, Ready, plus interpolation,
blur, mask model, overlay quads, and serials.

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

No PlayerController, building-placement, TEMP HUD, gameplay authority, or replication policy changed,
so their unrelated contracts were not escalated.

## Operator test

1. Start normal Team 1 PIE.
2. Confirm starting sight regions are normal and untouched territory is black.
3. Move a Worker or LongRange unit outward; black cells become normal Visible.
4. Move away; previously seen cells become dim/grey, never black.
5. Return; dim/grey cells become normal Visible.
6. Pan, zoom, and rotate; confirm the mask remains aligned.
7. Damage an own unit/building: bar appears; heal to full: bar hides; death/zero remains hidden.
8. In two-player listen-server PIE, move a damaged enemy between Visible and hidden cells. Confirm
   mesh/team/bar hide in Explored/Unexplored and restore with current state in Visible.
9. Inspect Visible/Explored/Unexplored borders at normal zoom: an isolated sight region should read as
   a soft rounded reveal, not a 200 cm staircase. Overlapping sources should merge softly; a moving
   explored trail should look like a blurred corridor (~160 cm transition).
10. Pan, zoom, and yaw rapidly, including past the previous sampled area: the mask must stay present
    (not full-black). `gp.FoW.VisualDump` should show `BilinearUpsampleSeparableBoxBlur`,
    PresentationSupersample=10, PresentationTexelWorldSize=20, BlurRadiusSamples=8, FallbackActive=false.
11. Toggle `gp.FoW.VisualEnable 0`, then `1`, for A/B confirmation.
12. Confirm Team 1 and Team 2 still show different local masks.

## Deferred

- enemy actor replication relevance (`IsNetRelevantFor`) and last-known representation
- hidden selection/inspect policy
- last-known static/dynamic state
- explicit Attack last-known behavior
- minimap/minimap FoW
- full production HUD, Order Menu, notifications, end screen
- DropPod temporary sight

No authored asset was created or modified.

**NOT MERGED. NOT FINALIZED.**
