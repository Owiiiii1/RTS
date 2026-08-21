# Cursor Work Report — Production HUD ViewModel Bridge and Bootstrap

## Status

**HUD_VIEWMODEL_BRIDGE_AND_BOOTSTRAP_READY_FOR_OPERATOR_VALIDATION**

## Branch / base / head

- Branch: `feature/gp-production-hud-viewmodel-bridge`
- Base: `origin/main` @ `61cedc682a391225ac0a02a716f3d36a4c176d7e`
- Head: `1a13d0bf9cf2d2372d34ddb07fd8cba5e6ed6349`
- **NOT MERGED**
- **NOT FINALIZED**

## Bootstrap owner

`UGP_HUDViewModelSubsystem` (`ULocalPlayerSubsystem` in GPUIRuntime).

No second LocalPlayer UI owner. `AGP_PlayerController` is unchanged and does not reference GPUIRuntime types. `GPRuntime` does not depend on `GPUIRuntime`. TEMP HUD remains created by GPRuntime PlayerController.

## Settings class / property

- Class: `UGP_UIPresentationSettings` (`UDeveloperSettings`, Config=Game, DefaultConfig)
- Property: `ProductionHUDWidgetClass` (`TSoftClassPtr<UGP_HUDRootWidget>`)
- Editor path: Project Settings → Game → GP UI Presentation
- This slice does **not** write `GP/Config`
- No hardcoded `/Game/.../WBP_GP_HUD` path
- Unconfigured class: safe no-op + non-shipping warning; TEMP HUD remains available

Operator assigns authored `WBP_GP_HUD` to this setting locally after the code lands.

## Lifecycle

Create only for a valid LocalPlayer with a local PlayerController.

Hooks:

- `Initialize` → `EnsureProductionHUD`
- `PlayerControllerChanged` → ensure if local controller, otherwise teardown
- `Deinitialize` → teardown first

No Tick. No timer retry. No world actor scan.

## Duplicate prevention

If the existing production HUD instance is valid, matches the resolved class, and is owned by the current local controller: re-add to viewport only if missing. Otherwise teardown then create once.

## Teardown

`RemoveFromParent()` on the production HUD, then clear `ProductionHUDWidget`.

## Input

Root is `HitTestInvisible` and not focusable. No controller input-mode change. RTS world clicks are not blocked. Readout/layout only.

## Debug command

`gp.UI.HUDStatus`

Prints:

- LocalPlayer present
- configured production HUD class
- production HUD instance present
- widget class/name
- ViewModel subsystem Ready state

Does not bypass widget bootstrap.

## Bridge API

Unchanged from the prior slice:

- `FGP_HUDViewModelBridge::AssignOwnedViewModels`
- `FGP_HUDViewModelBridge::AssignOwnedViewModelsToView`
- Slots: `GP_ResourceViewModel`, `GP_MatchViewModel`
- `UGP_HUDRootWidget::NativeConstruct` assigns subsystem-owned VMs after Super

## Tests

| Command | Result |
| --- | --- |
| `gp.UI.RunProductionHUDFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDBootstrapContractTest` | **PASS**, Failures=0 |

Bootstrap contract proves: unconfigured/null class no-op, at most one HUD per local player, repeated ensure does not duplicate, teardown clears/removes, TEMP HUD path remains, no GPRuntime→GPUIRuntime type ownership.

## GPEditor / UHT

`Build.bat GPEditor Win64 Development` — **PASS** (UHT processed GPEditor; compile/link succeeded).

GP Win64 Development / Shipping **not run**.

## Exact changed files

- `GP/Source/GPUIRuntime/GPUIRuntime.Build.cs`
- `GP/Source/GPUIRuntime/Public/Settings/GPUIPresentationSettings.h`
- `GP/Source/GPUIRuntime/Private/Settings/GPUIPresentationSettings.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDRootWidgetContractStub.h`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDBootstrapContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Content / TEMP HUD / merge

- **Content untouched** (`WBP_GP_HUD` and all `GP/Content` unstaged)
- `GP/Config` untouched in this slice
- **TEMP HUD preserved** (`UGP_TEMP_S28P_PlanetaryFerroniteHUD` still created by `AGP_PlayerController`)
- **NOT MERGED**
- **NOT FINALIZED**
