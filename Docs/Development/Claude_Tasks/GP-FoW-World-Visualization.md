# GP — Fog of War World Visualization

**Status:** `FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION`
**Branch:** `feature/gp-fow-world-visualization`
**Base:** `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Goal

Make the trusted one-team local FoW physically visible over the current MVP arena:

- Unexplored: opaque black;
- Explored: dim/grey terrain;
- Visible: unchanged SceneColor.

This slice is presentation-only. Gameplay FoW remains **200 cm / 5 Hz / 1000×1000**.

## Operator FAIL of the previous post-process head

Enemy hiding worked. Terrain fog did not visibly apply (map stayed fully lit). The game stuttered.

Root causes:

- view-family `RenderTarget` pointer identity was not the LocalPlayer game view, so `AddBlendable()` often never ran;
- `UMaterialExpressionWorldPosition` at after-tonemap sampled the post-process quad, not scene pixels;
- CPU encoded via ~1M world queries, four float planes, CPU blur, CPU temporal lerp, and two texture uploads.

## Corrected architecture

`UGP_FoWWorldPresentationSubsystem` remains the local-player owner.

- Local view identity: `FSceneView::PlayerIndex` vs `ULocalPlayer::GetControllerId()` / `GetLocalPlayerIndex()`, plus ViewActor fallback. Camera `AddOrUpdateBlendable` is the secondary local-only route.
- `PostProcessBound` means actual owned-view injection (`BlendableInjectionCount > 0`).
- CPU: `BuildPresentationMaskRGBA` packed 1000×1000 RG-in-BGRA8, one texel per cell.
- GPU: bilinear + 9-tap spatial; `lerp(Previous, Target, BlendAlpha)` temporal over 0.20 s.
- Ping-pong: one target upload per revision.
- Scene-pixel world XY from SceneDepth + `SvPositionToTranslatedWorld`.
- `gp.FoW.VisualDebugMode 1` full-screen diagnostic tint; `0` is normal FoW.
- `gp.FoW.VisualEnable 0` removes terrain fog only.

Measured contract encode/upload: **MaskEncodeMs=3.422**, **MaskUploadMs=0.912**.

## Diagnostics

`gp.FoW.VisualDump` reports Renderer, MaskModel, TextureResolution, WorldOrigin, WorldExtent,
MaskRevision, BlendAlpha, SpatialFilter=`GPUBilinear9Tap`, TemporalFilter=`GPULerpBlendAlpha`,
MaskEncodeMs, MaskUploadMs, PostProcessBound, ActualViewsSeen, BlendableInjectionCount,
LastInjectedFrame, LastInjectedView, CameraBlendable, DebugMode, WorldPosition method.

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

The only authored asset is `/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess`.
Existing maps, Blueprints, DataAssets, materials, VFX, Config, and Tools were not modified.

**NOT MERGED. NOT FINALIZED.**
