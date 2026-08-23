# Cursor Work Report — Production HUD Launch Menu Finalization

## Status

**PRODUCTION_LAUNCH_MENU_FINALIZED_READY_TO_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `cleanup/gp-remove-temp-hud`
- Base: `origin/main` @ `7fc2ab9ee880e73e1c7610570cc9a723b9376905`
- Parent: `7ff4a1226abd8b42506f5db4c273f79f144ef63b` (`SelfHitTestInvisible` interaction fix)
- Head: this finalization commit on `cleanup/gp-remove-temp-hud` (pushed with this report)

## Operator PASS details

Operator PIE validation **PASSED**. Confirmed live:

- obsolete TEMP HUD is retired
- Production HUD remains active and updates correctly
- right-side Launch menu is authored in operator-local `WBP_GP_HUD`
- all MainBase container rows appear
- fill bars update with storage state
- partial containers display yellow fill
- Ready/full containers display green fill
- Launch button is disabled when no launch is available
- Launch button becomes enabled when a Ready container exists
- Launch button is clickable after the root visibility fix
- clicking Launch executes the existing container launch gameplay path
- container/storage presentation updates correctly after launch

Operator-authored Blueprint assets remain local and were not touched or staged.

## TEMP HUD retirement summary

Deleted:

- `GP/Source/GPRuntime/Public/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`

No runtime creation of `UGP_TEMP_S28P_PlanetaryFerroniteHUD` remains. PlayerController TEMP presentation ownership is gone. Production HUD is the active match HUD (`UGP_HUDViewModelSubsystem` → configured `ProductionHUDWidgetClass` → `WBP_GP_HUD`).

## LaunchMenuPresenter ownership / data flow

- Owned by `UGP_HUDViewModelSubsystem` (LocalPlayer). Not PlayerController presentation state.
- Gameplay SoT: local team → resolved `AGP_MainBase` → `UGP_StorageComponent`
- Event-driven: `OnResolvedMainBaseChanged` (local team only) + local `OnStorageChanged`
- MainBase replacement unbinds old storage and binds the new component
- Other-team storage is ignored
- No Tick, no timer, no polling, no world scan
- Row count follows factual storage container count
- `FillNormalized` = clamp(StoredAmount / Capacity, 0..1)
- `bIsReadyForLaunch` from `EGP_StorageContainerState::Ready`
- `CanLaunchReadyContainer` = ready count > 0 and `!IsLaunchInFlight()`

## HUDRoot Blueprint-facing API

On `UGP_HUDRootWidget`:

- `GetLaunchContainerRows()` / `GetLaunchContainerPresentations()`
- `CanLaunchReadyContainer()`
- `GetReadyLaunchContainerCount()`
- `RequestLaunchReadyContainer()`
- `BP_OnLaunchMenuChanged()` (`BlueprintImplementableEvent`)

WBP color contract: yellow while `!bIsReadyForLaunch`, green when Ready/full.

## SelfHitTestInvisible interaction fix

Operator first found Launch OnClicked dead while enablement was correct. Root cause: production HUD root was `HitTestInvisible`, which blocks hit-testing for the widget and children.

Fixed in every bootstrap/re-add path of `UGP_HUDViewModelSubsystem::EnsureProductionHUDInternal` to `SelfHitTestInvisible`:

- root/background does not consume pointer hits
- interactive descendants (`BTN_Launch`, future procurement buttons) remain clickable
- no input-mode change
- no mouse-capture redesign

## Exact authoritative launch forwarding path

`UGP_HUDRootWidget::RequestLaunchReadyContainer()`
→ `AGP_PlayerController::RequestLaunchReadyContainer()`
→ `Server_RequestLaunchReadyContainer`
→ `AuthorityTryLaunchReadyContainerForOwningTeam`

Gameplay/RPC APIs preserved. Storage, orbital transfer, and economy semantics unchanged.

## Focused contract results

All **Complete Failures=0**:

- `gp.UI.RunLaunchMenuPresentationContractTest` — Cancelled=false
- `gp.UI.RunProductionHUDFoundationContractTest` — Cancelled=false
- `gp.UI.RunHUDViewModelBridgeContractTest` — Cancelled=false
- `gp.UI.RunHUDBootstrapContractTest` — Cancelled=false
- `gp.UI.RunPlanetFerronitePresentationContractTest` — Cancelled=false
- `gp.UI.RunThreatPresentationContractTest` — Cancelled=false
- `gp.Resource.RunContainerLaunchContractTest` — Cancelled=false, `SuiteComplete`
- `gp.Resource.RunContainerLaunchHUDContractTest` — Cancelled=None (`None` = not cancelled), `SuiteComplete`

Full suite not run.

## GPEditor / UHT result

`GPEditor Win64 Development` **Result: Succeeded** (UHT compiled-in).

## GP Development result

`GP Win64 Development` **Result: Succeeded**. Output: `GP/Binaries/Win64/GP.exe`.

## GP Shipping result

`GP Win64 Shipping` **Result: Succeeded**. Output: `GP/Binaries/Win64/GP-Win64-Shipping.exe`.

## Factual final diff review vs `origin/main`

Confirmed:

- TEMP HUD class remains deleted
- no TEMP HUD runtime creation remains
- PlayerController gameplay/RPC APIs preserved (`RequestLaunchReadyContainer`, unit drop, building purchase, wall package)
- Production HUD bootstrap remains LocalPlayer-owned
- Production HUD root uses `SelfHitTestInvisible`
- no input-mode changes
- LaunchMenuPresenter is event-driven
- no Tick / timer / polling / world scan in launch-menu presentation
- presenter binds only local MainBase storage
- MainBase replacement unbinds old storage and binds new storage
- container row count follows factual storage container count
- `FillNormalized` remains clamped 0..1
- Ready state comes from actual storage container state
- `CanLaunchReadyContainer` respects ready count and launch-in-flight
- HUDRoot `RequestLaunchReadyContainer` forwards to existing PlayerController path
- storage/orbital/economy gameplay semantics were not changed (`GPStorageComponent.h` only dropped TEMP HUD weak-ptr from a contract runner and updated a comment)
- no protected Content/Config/map/DataAsset/Tools files are committed

No defect found that required a code fix in this finalization pass.

## Exact changed files (branch vs `origin/main`)

Deleted:

- `GP/Source/GPRuntime/Public/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`

Added:

- `GP/Source/GPUIRuntime/Public/ViewModels/GPLaunchMenuPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPLaunchMenuPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPLaunchMenuPresentationContractTest.cpp`

Modified:

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Resources/GPStorageComponent.h`
- `GP/Source/GPRuntime/Private/Debug/GPContainerLaunchHUDContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMatchWinLoseContractTest.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDBootstrapContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPProductionHUDFoundationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected files untouched

Not modified or staged by this branch:

- `GP/Content/**`
- `GP/Config/**`
- maps
- DataAssets
- Tools
- `WBP_GP_HUD`
- `WBP_GP_LaunchContainerRow`

Local dirty authored Content/Config/`GP.uproject`/`Tools` remain unstaged operator work.

## No gameplay / economy semantic changes

Storage rules, orbital transfer, economy values, and container launch gameplay are unchanged. This branch retires TEMP HUD presentation, adds Production HUD launch-menu presentation, and fixes HUD-root hit-testing.

## Merge state

**NOT MERGED.**
