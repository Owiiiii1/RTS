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
- Mirror update/reset events increment a render serial and invalidate cached presentation.
- The volatile widget observes view projection each paint; it only rebuilds geometry when the mirror
  serial or camera/view projection changes.
- It deprojects the player view to the Z=0 gameplay plane, samples only the intersecting FoW cells
  (plus a one-cell LocalFoW pad), and builds a cell-center scalar field.
- Dual marching squares interpolates between neighboring centers. Mixed dual quads are subdivided 4×
  so bilinear isos become curved contours instead of 200 cm square outlines.
- Uniform interiors coalesce into Slate triangles. Visible dual samples create a hole; Unexplored and
  Explored fill the overlay.
- Iso vertices use `ConservativeBoundaryT=0.42` from the clearer center toward the darker neighbor.
- NotReady, projection failure, or an over-budget view falls back to full-screen black.

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
  the 200 cm cell silhouette intact. Replaced by dual marching-squares contour reconstruction.

## Visual and performance contract

- Unexplored obscuration: `1.0`, color black.
- Explored obscuration: `0.68`, neutral near-black tint.
- Visible obscuration: `0.0`, no quad.
- Gameplay cells remain 200 cm; no gameplay visibility expansion or state interpolation occurs.
- Presentation reconstructs a continuous contour from discrete LocalFoW cell-center samples. Dual
  marching squares with 4× mixed-quad subdivision interpolates diagonal/circular boundaries so they
  no longer follow cell-square outlines.
- Iso bias `ConservativeBoundaryT=0.42` (`< 0.5`) keeps every contour on the clearer side of a dual
  edge: visual Visible may shrink slightly, but Unexplored cell centers stay fully obscured.
- Renderer owns no duplicate gameplay grid and performs no sight-circle computation.
- No full one-million-cell presentation copy is allocated.
- View sampling is capped at 65,536 cells.
- Overlay geometry is capped at 65,536 triangles and 32,768 iso segments; coalesced interiors use
  up to 8,000 quads per Slate draw batch.
- Static camera + unchanged revision reuses cached vertices; camera changes rebuild only the bounded
  view region.
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

`VisualDump` reports active/enabled/ready state, team/revision, contour algorithm, metadata, sample
cap, current sampled/padded region, contour segment and overlay vertex/triangle counts, mixed dual
quads, coalesced interiors, conservative T, subcells-per-cell, registered-unit count, 10 Hz
evaluation interval, dirty/cached serials, and consumed render serial.

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
   a smooth circle/curve, not a 200 cm checker of blurred squares. Overlapping sources should merge;
   a moving explored trail should look like a rounded corridor.
10. Run `gp.FoW.VisualDump`; confirm Active/Ready/team/revision, algorithm
    `ConservativeDualMarchingSquares`, conservative T=0.42, subcells=4, and bounded
    sampled/contour/triangle stats.
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
