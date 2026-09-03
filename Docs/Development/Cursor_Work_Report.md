# Cursor Work Report

## Status

**MINIMAP_ENEMY_BLIPS_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Minimap blips now use canonical player/team color. Unit vs building is marker size only. Friendly actors stay visible inside playable bounds. Enemy units/buildings appear only while currently Visible on trusted local FoW. Last-known is not in this checkpoint.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `157d6b7` (friendly-blips SHA record) |
| Checkpoint HEAD | `b0ce02bda1fe5100b26efa4a61974792d5b023b6` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Canonical team color source

`UGP_GameplayPresentationSettings::GetTeamColor(TeamId)` is the single source of truth (same mapping as `UGP_TeamPresentationComponent` / unit visuals). Unknown or unassigned TeamId (`< 1` or unlisted) resolves to `NeutralTeamColor`.

Presentation data stores `FGP_MinimapBlip::TeamId`. `UGP_MinimapWidget::ResolveBlipColor` reads the settings at paint time. There is no separate friendly/enemy color scheme.

Removed widget-local `FriendlyUnitBlipColor` (cyan) and `FriendlyBuildingBlipColor` (yellow).

## Unit / building size semantics

Same TeamId → same color for units, buildings, and MainBase.

Type differentiation is size only:

- unit half-extent `1.75px`
- building/MainBase half-extent `2.75px`

## Enemy FoW gating source

Same live membership as friendlies: `UGP_LocalFoWUnitPresentationSubsystem::RegisteredUnits` (`AGP_UnitBase` BeginPlay/EndPlay). No second registry. No actor scans.

Gating reuses `UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer` against the presenter's trusted `UGP_LocalFoWComponent`:

- TeamId `< 1`: not classified as friendly or enemy; omitted
- Friendly (`TeamId == LocalTeamId >= 1`): always shown inside camera/playable bounds
- Enemy (`TeamId >= 1` and `!= LocalTeamId`): shown only if `IsVisible` (Visible). Explored and Unexplored omit the blip

No distance-based enemy visibility. No last-known markers.

## Update lifecycle

No widget Tick. No world scan.

| Event | Path |
| --- | --- |
| Register / unregister | `OnUnitRegistryChanged` → `RebuildBlips` |
| Movement / visibility edge | existing 10 Hz `EvaluateRegisteredUnits` → `OnRegisteredUnitsEvaluated` |
| FoW revision | `OnLocalFoWUpdated` → `RebuildPresentation` → `RebuildBlips` |

Enemy Visible → blip appears; leaving Visible → blip disappears. Bounded O(registered actors).

Paint order (one Slate pass, shared `Xscreen = 1-X`, `Yscreen = 1-Y`):

1. background
2. FoW
3. friendly + currently-visible enemy blips

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | **PASS** Complete Failures=0 |

Focused coverage: same-TeamId unit/building same resolved color; different TeamId different configured color; size not color; no cyan/yellow type split; enemy Visible present; Explored/Unexplored absent; Visible→Explored removes; Explored→Visible restores; enemy building same rules; friendly remains regardless FoW; outside bounds omitted; invalid TeamId omitted.

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

## Changed files (this checkpoint)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPMinimapPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMinimapPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapEnemyBlipsContractTest.cpp` (new)
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not committed / not modified for this checkpoint:

- `WBP_GP_HUD`
- `GP/Content/`
- `GP/Config/`
- `L_PrototypeArena`
- DataAssets
- Materials/VFX
- `GP.uproject`
- `Tools/`

Operator dirty/untracked Content/Config preserved.

## Operator test

1. PIE on `L_PrototypeArena`
2. Confirm own units and buildings are the same team color
3. Confirm buildings are larger dots, not a different color
4. Confirm an enemy blip appears only when that enemy is Visible
5. Move the enemy out of vision → blip disappears
6. Reveal the enemy again → blip reappears

## Out of scope (next checkpoints)

- Last-known markers
- Camera rectangle
- Click-to-pan
- Selection highlighting
- Per-type unit icons
- SceneCapture / terrain / voxel

**INTERMEDIATE / NOT MERGE READY**
