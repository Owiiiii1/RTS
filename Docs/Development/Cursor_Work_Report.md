# Cursor Work Report

## Status

**MINIMAP_STAGE_MERGE_READY**

Operator PASS is confirmed for the full minimap stage. Finalization audit, contracts, GPEditor, GP Development, GP Shipping, graphics smoke, and docs are PASS. `origin/main` did not move. Do not change `ui/gp-minimap` until merge.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-finalization HEAD | `83f6cd7d7b900b35f76b7ee9540439a3630302ff` |
| `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |
| Ahead / behind vs `origin/main` | **23 / 0** after the merge-ready commit (behind **0**) |
| Finalization commit | `e969a7216a7a1fa42003dddb6e3e72b89a83086f` |

After fetch: `origin/main` equals merge-base. No main integration required. No rebase/reset/stash.

## Architecture summary

GPRuntime owns camera navigation and trusted presentation sources. `AGP_CameraPawn` is the canonical camera/playable bounds owner (`GetResolvedCameraBounds`, `OnResolvedCameraBoundsChanged`). `SetCameraAnchorWorldXY` is the local HUD pan API: actor XY-anchor only, Z/yaw/zoom/pitch preserved, `ClampToBounds`, spring-arm + component world sync, then `OnCameraPresentationChanged`. No RPC. `GetPresentationView()` is the current CameraComponent location/rotation/FOV. Blips consume the existing `UGP_LocalFoWUnitPresentationSubsystem` registry. FoW gameplay grid stays 2000×2000 / 100 cm.

GPUIRuntime owns presentation only. `UGP_MinimapPresenter` on `UGP_HUDViewModelSubsystem` binds LocalFoW, camera bounds, camera presentation, and the unit registry. Displayed map rect = resolved camera/playable bounds. One shared MapDest / surface transform (`Xscreen = 1 - NormalizedX`, `Yscreen = 1 - NormalizedY`). `UGP_MinimapWidget` + `SGPMinimapSurface` paint background, FoW downsample, blips, and camera footprint. HUD root bridges presenter accessors. Settings async-load `MinimapBackgroundTexture`. Click-to-pan is widget → `PanCameraToMinimapNormalized` → `SetCameraAnchorWorldXY`, never `SetActorLocation` from the widget.

Confirmed non-goals in production:

- no widget Tick
- no polling timer
- no world actor scans in UI
- no SceneCapture
- no RenderTarget
- no synchronous asset load
- no gameplay authority in minimap
- no RPC for camera navigation
- no duplicated FoW gameplay state
- no duplicate unit registry
- one shared map rect
- one shared surface transform
- current CameraComponent view for footprint
- camera bounds as canonical displayed-world source

`GPUIRuntime.Build.cs` keeps **public** `InputCore` because `GPMinimapWidget.h` includes `InputCoreTypes.h` and exposes `FKey` on the public widget API. Private would not compile that header. `GPRuntime` public `InputCore` is pre-existing camera/input, not a minimap-only add.

## Operator-PASS features

- minimap surface / palette
- camera/playable bounds crop
- static background/fallback
- FoW overlay (Unexplored / Explored / Visible)
- orientation `(1-X, 1-Y)`
- friendly blips
- enemy Visible-only blips
- canonical team colors
- building/unit size differentiation
- camera projected footprint
- camera-footprint crash fix
- LMB click-to-pan
- immediate footprint sync after click-to-pan

## Final code audit

Branch diff `origin/main...HEAD` is docs + GPRuntime camera/FoW registry seams + GPUIRuntime presenter/widget/HUD/settings/tests. No Content, Config, maps, binaries, Saved, or Intermediate.

Findings:

- No remaining `TArray` self-reference Add on the footprint outline path (crash already fixed by copy-then-Add).
- Delegate binds Unbind-before-bind with same-object short-circuit; `ReleaseSlateResources` clears the Slate LMB callback and unbinds before Super.
- Slate LMB uses a weak `CreateUObject` callback into the UWidget; UObject lifetime is not owned by Slate.
- NaN / zero-size / degenerate-bounds / `< 3` clip-point guards remain on mapping and footprint.
- No widget Tick, no poll timer, no per-frame world scan.
- Contract seams and `FAutoConsoleCommand` tests are `#if !UE_BUILD_SHIPPING`.
- No leftover PlayerCameraManager / `DeprojectScreenPositionToWorld` production path. Finalization removed the stale `Camera/PlayerCameraManager.h` include from `GPMinimapPresenter.cpp`.
- No duplicate coordinate converters beyond presenter normalize + widget surface inverse.
- No hardcoded cyan/yellow semantic team colors; paint uses `UGP_GameplayPresentationSettings::GetTeamColor`.
- No SceneCapture / RenderTarget / `LoadSynchronous` production path.

Cleanup done: unused PCM include only. No broad refactor.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<one cmd>` (no comma-joined ExecCmds).

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapClickToPanContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapClickToPanFootprintSyncContractTest` | Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | Complete Failures=0 |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | Complete Failures=0 |

No `gp.Camera.*` console contracts exist. Marquee is the nearest input/LMB-consume regression for click-to-pan consume-without-fallthrough.

## Builds

| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** (`Result: Succeeded`) |
| GP Win64 Development | **PASS** (`GP.exe`) |
| GP Win64 Shipping | **PASS** (`GP-Win64-Shipping.exe`) |

Shipping did not expose debug/test leakage.

## Runtime smoke

`UnrealEditor-Cmd` `L_PrototypeArena` `-game` **without NullRHI** (D3D12, NVIDIA GeForce RTX 4070 SUPER).

- LoadMap `/Game/GrimProtocol/Maps/L_PrototypeArena` succeeded (`LogLoad: Took 0.522472 seconds`)
- No `Fatal error` / `Assertion failed`
- `gp.UI.HUDStatus`: `LocalPlayer=LocalPlayer_0` `ConfiguredClass=.../WBP_GP_HUD_C` `InstancePresent=true` `Ready=Ready`

HUD/minimap subsystem initialization passed. Operator visual PASS already covers the painted surface.

## Docs updated

- `Docs/TDD/11_RTS_Camera.md` — camera bounds, `GetPresentationView`, `SetCameraAnchorWorldXY`, deferred drag-pan / terrain-aware projection
- `Docs/TDD/12_UI_Architecture.md` — MVP minimap surface complete; deferred extras listed
- `Docs/TDD/15_Fog_of_War.md` — click-to-pan is shipped; last-known remains deferred; no SceneCapture
- `Docs/GDD/09_UI_UX.md` — current surface vs deferred deposit/SWARM/last-known/drag-pan
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md` — Stage 2 Minimap + FoW minimap presentation **COMPLETE**; next allowed stage **3. Terrain / Voxel / Foundation 3A–3E**

Voxel was not started.

## Protected operator files

No `git reset --hard`, `git clean`, stash, or broad restore.

**Before finalization commit:**

```
 M GP/Config/DefaultEngine.ini
 M GP/Config/DefaultGame.ini
 M GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap
 M GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset
 M GP/GP.uproject
?? GP/Content/Basic_VFX/
?? GP/Content/GrimProtocol/Blueprint/
?? GP/Content/GrimProtocol/DataAssets/Buildings/
?? GP/Content/GrimProtocol/DataAssets/Game/
?? GP/Content/GrimProtocol/DataAssets/Units/
?? GP/Content/GrimProtocol/Materials/
?? GP/Content/Mixed_Magic_VFX_Pack/
?? GP/Content/RocketThrusterExhaustFX/
?? Tools/
```

**After finalization commit (`e969a72`):** same protected dirty/untracked set, unchanged:

```
 M GP/Config/DefaultEngine.ini
 M GP/Config/DefaultGame.ini
 M GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap
 M GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset
 M GP/GP.uproject
?? GP/Content/Basic_VFX/
?? GP/Content/GrimProtocol/Blueprint/
?? GP/Content/GrimProtocol/DataAssets/Buildings/
?? GP/Content/GrimProtocol/DataAssets/Game/
?? GP/Content/GrimProtocol/DataAssets/Units/
?? GP/Content/GrimProtocol/Materials/
?? GP/Content/Mixed_Magic_VFX_Pack/
?? GP/Content/RocketThrusterExhaustFX/
?? Tools/
```

Protected set is preserved. Not committed: `WBP_GP_HUD`, launch/selection/purchase rows, `GP/Content/`, `GP/Config/`, `L_PrototypeArena`, DataAssets, Materials/VFX, `GP.uproject`, `Tools/`.

## Final diff audit (`origin/main..HEAD`)

30 committed files before this finalization: docs + camera/FoW presentation seams + minimap UI + tests. Finalization adds docs wording + unused PCM include removal + this report.

No accidental map/assets/config/binaries/Saved/Intermediate/operator assets.

## Known intentionally deferred

- minimap drag-pan
- RMB minimap orders
- pings
- selection via minimap
- last-known enemy markers
- per-type icons
- SceneCapture
- terrain/voxel-aware footprint

## Merge recommendation

Merge `ui/gp-minimap` into `main` with a regular merge (no rebase, no force-push). Branch is behind 0. Keep operator dirty/untracked out of the merge. After merge, next allowed roadmap stage is Terrain 3A–3E; do not start Voxel from leftover minimap work.
