# Cursor Work Report — Production HUD Right-Side Launch Menu

## Status

**PRODUCTION_LAUNCH_MENU_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

## Branch / base / head

- Branch: `cleanup/gp-remove-temp-hud`
- Base: `origin/main` @ `7fc2ab9ee880e73e1c7610570cc9a723b9376905`
- Parent: `28ca7ef16ab1312e39f2320b78b78fe9e0e3f4ec` (TEMP HUD retirement)
- Head: this implementation commit on `cleanup/gp-remove-temp-hud` (pushed with this report)

Continued from the TEMP HUD retirement commit on this branch. Did not reset to stale `main`.

## Exact presenter / ownership shape

Gameplay source of truth is unchanged: local team → resolved `AGP_MainBase` → `UGP_StorageComponent`.

Presentation is LocalPlayer-owned, not PlayerController-owned:

- `UGP_HUDViewModelSubsystem` (LocalPlayer subsystem) creates and owns `UGP_LaunchMenuPresenter`
- Presenter is created in subsystem `Initialize`, `Shutdown` on `Rebind` / `Deinitialize`, `Initialize(GameState, LocalTeamId)` after local team is valid
- `UGP_HUDRootWidget` binds the presenter on `NativeConstruct` and unbinds on `NativeDestruct`
- Authored `WBP_GP_HUD` layout remains operator-local and uncommitted

`FGP_LaunchContainerRow` (Blueprint-read-only):

- `Index`
- `StoredAmount` (`FGP_StorageContainer.CurrentAmount`)
- `Capacity` (`UGP_StorageComponent::GetContainerCapacity()`)
- `FillNormalized` = clamp(`StoredAmount` / `Capacity`, 0..1)
- `bIsReadyForLaunch` = (`State == EGP_StorageContainerState::Ready`)

`CanLaunchReadyContainer` = `GetReadyCount() > 0 && !IsLaunchInFlight()`.

No Tick, no timers, no polling, no world scan.

## Exact Blueprint-facing HUDRoot API

On `UGP_HUDRootWidget`:

- `GetLaunchContainerRows()` → `TArray<FGP_LaunchContainerRow>`
- `GetLaunchContainerPresentations()` → same array (alias for WBP authoring)
- `CanLaunchReadyContainer()` → `bool`
- `GetReadyLaunchContainerCount()` → `int32`
- `RequestLaunchReadyContainer()` → `void`
- `BP_OnLaunchMenuChanged()` → `BlueprintImplementableEvent`

WBP color choice: yellow while `!bIsReadyForLaunch`, green when `bIsReadyForLaunch`.

## Exact event / delegate sources

Local team only; other teams are ignored.

- `AGP_GameState::OnResolvedMainBaseChanged` → rebind local storage when `TeamId == LocalTeamId`
- Local `UGP_StorageComponent::OnStorageChanged` → rebuild rows
- Presenter `OnLaunchMenuPresentationChanged` → `UGP_HUDRootWidget::BP_OnLaunchMenuChanged`
- Also fires on presenter initialize / shutdown / reset and once when the HUD root binds

Local MainBase lookup uses `FindMainBaseForTeamClientSafe`. Shutdown/rebind unbinds first; repeated `Initialize` does not duplicate delegates.

## Exact launch request forwarding path

`UGP_HUDRootWidget::RequestLaunchReadyContainer()`
→ `AGP_PlayerController::RequestLaunchReadyContainer()`
→ `Server_RequestLaunchReadyContainer`
→ `AuthorityTryLaunchReadyContainerForOwningTeam`

No gameplay launch logic was duplicated or changed.

## Focused contract results

`gp.UI.RunLaunchMenuPresentationContractTest` — **Complete Failures=0 Cancelled=false**

PASS:

- no Tick on presenter/subsystem
- TEMP HUD remains retired
- HUD root launch-menu API present
- `Server_RequestLaunchReadyContainer` remains
- no local MainBase/storage → empty list, ready count 0, can launch false
- local storage with 5 containers → 5 rows
- `FillNormalized` correct and clamped
- `bIsReadyForLaunch` matches full/ready state
- `ReadyContainerCount` correct
- other team storage does not affect local launch menu
- local MainBase replacement rebinds cleanly
- old storage no longer updates presentation after rebind
- repeated Initialize does not duplicate delegates
- HUD `RequestLaunchReadyContainer` forwards without crash
- Shutdown clears presentation

## Regressions

All **Complete Failures=0 Cancelled=false**:

- `gp.UI.RunProductionHUDFoundationContractTest`
- `gp.UI.RunHUDViewModelBridgeContractTest`
- `gp.UI.RunHUDBootstrapContractTest`
- `gp.UI.RunPlanetFerronitePresentationContractTest`
- `gp.UI.RunThreatPresentationContractTest`

Launch / orbital gameplay contracts actually executed (not teardown stubs):

- `gp.Resource.RunContainerLaunchContractTest` — **Complete Failures=0 Cancelled=false**, `SuiteComplete`
- `gp.Resource.RunContainerLaunchHUDContractTest` — **Complete Failures=0 Cancelled=None** (`None` = not cancelled), `SuiteComplete`

GP Development / Shipping not run.

## GPEditor / UHT result

`GPEditor Win64 Development` **Result: Succeeded** (UHT compiled-in). Focused rebuild compiled presenter, HUD root, subsystem, and launch-menu contract.

## Exact changed files

- `GP/Source/GPUIRuntime/Public/ViewModels/GPLaunchMenuPresenter.h` (new)
- `GP/Source/GPUIRuntime/Private/ViewModels/GPLaunchMenuPresenter.cpp` (new)
- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPLaunchMenuPresentationContractTest.cpp` (new)
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Content / protected files untouched

Not modified or staged by this slice:

- `GP/Content/**`
- `GP/Config/**`
- maps
- DataAssets
- Tools
- `WBP_GP_HUD` and other Blueprint/widget assets

Local dirty authored Content/Config/`GP.uproject`/`Tools` remain unstaged operator work.

## TEMP HUD remains retired

No restore of `UGP_TEMP_S28P_PlanetaryFerroniteHUD` or PlayerController TEMP presentation bars. Production HUD remains the active match HUD.

## No gameplay / economy semantic changes

Storage rules, orbital transfer, economy values, and container launch gameplay are unchanged. This slice is read-only presentation plus forwarding to the existing launch request API.

## Operator next step

Author the right-side vertical menu on `WBP_GP_HUD`: Launch button on top, container fill-bar list below, yellow while filling, green when ready. Bind to the HUD-root API above. Do not resurrect TEMP HUD.
