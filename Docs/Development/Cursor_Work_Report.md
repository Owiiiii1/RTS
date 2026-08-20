# Cursor Work Report — FoW Post-Process Binding + GPU Mask

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Previous failed operator head: `34086e8` / `f715ac7`
- Implementation head: this report-record commit

Gameplay FoW remains:

- CellSize = **200 cm**
- Dims = **1000×1000**
- Authoritative recompute = **0.20 sec / 5 Hz**

Do not change UnitDefinitions. Operator-local LongRange `Fog Of War Sight Radius = 2000` is preserved.

## Operator failure

1. **Terrain fog absent.** Enemy FoW presentation hiding worked (hidden enemy absent in non-Visible, appears when local player gains visibility). The level stayed normally visible: Unexplored was not black; Explored/Visible terrain distinction was effectively absent.
2. **Severe stutter.** The game noticeably hitching on LocalFoW revisions.

Both treated as implementation defects. Gameplay FoW resolution/rate was not changed.

## Root cause of post-process not visibly applying

Two independent defects:

1. **View binding.** `FGP_FoWSceneViewExtension::SetupView` required `InViewFamily.RenderTarget == LocalPlayer->ViewportClient->Viewport` before `AddBlendable()`. That pointer identity is not proof of the LocalPlayer game view in PIE/runtime, so the MID often never entered the rendered game view. `IsPostProcessBound()` previously meant only ViewExtension + MID + template existence, not actual injection.
2. **World position.** `UMaterialExpressionWorldPosition` at `BL_SceneColorAfterTonemapping` samples the post-process fullscreen primitive, not the scene pixel. Mask UV then tracks camera/quad position. If the camera sits in a Visible cell, the whole screen looks unfogged — matching the operator report.

Enemy hiding is a separate presentation gate (`UGP_LocalFoWUnitPresentationSubsystem`) and was never going through this material, which is why it still worked.

## Exact corrected local-view binding

UE 5.8 `ULocalPlayer::CalcSceneView` sets `ViewInitOptions.PlayerIndex = GetControllerId()` and `ViewActor = PlayerController->GetViewTarget()`.

Owned local game view now means:

- `InView.bIsGameView`
- not scene capture / not reflection capture
- `InView.PlayerIndex == LocalPlayer->GetControllerId()` **or** `GetLocalPlayerIndex()`
- fallback: `ViewActor` is the local ViewTarget, pawn, or PlayerController

The matching view receives that LocalPlayer's MID only (Team 1 MID / Team 2 MID; no global shared MID).

Secondary local-only route: the view-target `UCameraComponent` gets `AddOrUpdateBlendable(MID)` so PIE/standalone camera post-process also carries the owned MID. View-extension `SetupView` confirms or adds the blendable without double-applying if it is already present in `FinalPostProcessSettings`.

`gp.FoW.VisualEnable 0` skips injection and removes the camera blendable. Enemy hiding stays.

## Actual blendable injection proof

`PostProcessBound=true` now means a successful owned-view injection (`BlendableInjectionCount > 0`), not resource existence.

`gp.FoW.VisualDump` reports:

- `ActualViewsSeen=`
- `BlendableInjectionCount=`
- `LastInjectedFrame=`
- `LastInjectedView=`

Contract `X2` injects into a synthetic local game view using the same ownership function and proves the MID is in `FPostProcessSettings`. Debug mode `1` reaches the MID, then restores `0`.

## World-position reconstruction verdict

`UMaterialExpressionWorldPosition` is **not** used.

The material reconstructs **scene-pixel** world XY from:

- `PPI_SceneDepth` linear depth
- `ConvertToDeviceZ`
- `SvPositionToTranslatedWorld(SvPosition.xy, DeviceZ)`
- minus `ResolvedView.PreViewTranslation`

Mask UV = `(WorldXY - FoWOriginXY) * FoWInvExtentXY` for that reconstructed scene pixel, not camera/quad position.

## Old CPU cost analysis

On every LocalFoW revision the failed path did:

- ~1024×1024 `GetStateAtWorldLocation()` world queries
- four ~1M float planes (Previous/Target × Known/Visible)
- CPU separable box blur
- CPU million-element temporal lerp / bake
- RGBA pack
- **two** full texture uploads

That is the stutter.

## New bulk LocalFoW extraction

`UGP_LocalFoWComponent::BuildPresentationMaskRGBA(TArray<FColor>& OutPixels) const`

- read-only
- no TeamId query
- no mutation
- sequential bit-array walk, one texel per gameplay cell
- `R = Explored || Visible`, `G = Visible`

No 1M `GetStateAtWorldLocation` on the encode path.

## New packed mask representation

Runtime CPU state is the packed target pixel array only. No Previous/Target Known/Visible float[1M] planes.

## GPU spatial smoothing

Material 9-tap mask sample around UV, plus hardware bilinear. Params:

- `FoWMaskTexelSize`
- `FoWBlurRadiusTexels` (1 texel)

No CPU spatial blur at runtime.

## GPU temporal interpolation

Two GPU textures, ping-pong on revision:

1. swap Previous/Target resource pointers (previous becomes prior target)
2. upload the new exact mask into the new Target only
3. `BlendAlpha = 0`
4. shader `lerp(Previous, Target, BlendAlpha)` over **0.20 s**

Between revisions: scalar `BlendAlpha` only.

## Texture format / resolution

- Resolution: **1000×1000** (one texel = one LocalFoW cell). No 1000→1024 resample.
- Format: **BGRA8** (`PF_B8G8R8A8`), bilinear, clamp, non-sRGB. RG8 was not used because the proven transient `UpdateTextureRegions` path is BGRA8.
- ~4 MB per texture; one target upload per revision (~4 MB), not two.

Composition (GPU, same as CPU helper):

- Ready=false or Known≈0 → black
- Known≈1, Visible≈0 → SceneColor × 0.35
- Visible≈1 → SceneColor

## One-upload-per-revision proof

Contract `Z1`: capture Previous/Target pointers and upload count, ping-pong one packed upload, prove pointers swapped and `TargetUploadCount` increased by exactly 1.

## Measured EncodeMs / UploadMs

Contract-measured 1000×1000 packed encode + last target upload:

- **MaskEncodeMs = 3.422**
- **MaskUploadMs = 0.912**

Operator-visible stutter from million-element CPU blur/lerp/dual upload should be gone.

## Exact tests / build

| Command | Result |
| --- | --- |
| `gp.FoW.RunWorldVisualizationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunRuntimeFoundationContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunHealthBarContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunBuildGridContractTest` | **PASS**, Failures=0 |
| GPEditor Win64 Development + UHT | **PASS** |
| `-run=GPFoWPostProcessMaterialSeed` | **PASS** (result 0) |

No GP Win64 Development / Shipping.

## Changed files / assets

- `GP/Source/GPRuntime/Public/FogOfWar/GPLocalFoWComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPLocalFoWComponent.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWVisualMask.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWVisualMask.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPEditor/Private/FogOfWar/GPFoWPostProcessMaterialSeedCommandlet.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `GP/Content/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.uasset` (regenerated only)
- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md` (this report)

## Protected content

No edits under `GP/Config`, existing maps, Blueprints, DataAssets, materials, VFX, or `Tools` were staged or committed.

Existing local Config, map, Blueprint, DataAsset, material, VFX, Tools, and other Content changes remain unstaged and untouched. The operator-local LongRange Salvage Walker UnitDefinition with `Fog Of War Sight Radius = 2000` was not committed, reverted, restored, stashed, cleaned, or modified.

The only authored FoW material touched is `/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess`.

## Operator retest

1. Team 1 PIE: Unexplored black; Explored dim; Visible normal. No full-map unfogged terrain.
2. Optional `gp.FoW.VisualDebugMode 1` should full-screen tint, then `0` restores FoW.
3. Move units: terrain states update; camera pan/zoom/yaw does not rebuild the mask.
4. Enemy hiding still independent of terrain fog. `gp.FoW.VisualEnable 0` removes terrain fog only.
5. No hitch on FoW revision. `gp.FoW.VisualDump`: `TextureResolution=1000x1000`, `PostProcessBound=true`, `BlendableInjectionCount>0`, `SpatialFilter=GPUBilinear9Tap`, `CellSize=200`.

**NOT MERGED. NOT FINALIZED.**
