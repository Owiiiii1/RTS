# Cursor Work Report — Production HUD Launch Hit-Test Fix

## Status

**PRODUCTION_LAUNCH_MENU_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

## Branch / base / head

- Branch: `cleanup/gp-remove-temp-hud`
- Base: `origin/main` @ `7fc2ab9ee880e73e1c7610570cc9a723b9376905`
- Parent: `587d8ccd99ec89ae9ac7800e0f7df17ecf480b99` (launch menu presentation)
- Head: this implementation commit on `cleanup/gp-remove-temp-hud` (pushed with this report)

## Operator finding

Launch container rows worked and Launch became enabled when a container was Ready, but clicking Launch did nothing. Blueprint OnClicked was already wired to `UGP_HUDRootWidget::RequestLaunchReadyContainer()`.

## Confirmed root cause

`UGP_HUDViewModelSubsystem::EnsureProductionHUDInternal` set production HUD root visibility to `ESlateVisibility::HitTestInvisible` both when restoring an existing HUD to the viewport and after `CreateWidget` / `AddToViewport`.

In UMG, `HitTestInvisible` disables hit-testing for the widget **and its children**, so `BTN_Launch` never received pointer clicks.

The owning PlayerController was already passed to `CreateWidget`. The gameplay forward path was already correct.

## Fix

Changed every Production HUD bootstrap/re-add visibility assignment to:

`ESlateVisibility::SelfHitTestInvisible`

Semantics:

- HUD root/background itself does not consume pointer hits
- interactive child widgets remain hit-testable
- `BTN_Launch` OnClicked can run
- future production action/procurement buttons can also work
- no input-mode change
- no mouse-capture redesign
- no PlayerController gameplay changes

Did not set the whole HUD to `Visible`.

## Gameplay launch path unchanged

`UGP_HUDRootWidget::RequestLaunchReadyContainer()`
→ `AGP_PlayerController::RequestLaunchReadyContainer()`
→ `Server_RequestLaunchReadyContainer`
→ `AuthorityTryLaunchReadyContainerForOwningTeam`

## Focused contract results

- `gp.UI.RunHUDBootstrapContractTest` — **Complete Failures=0 Cancelled=false** (canonical visibility is `SelfHitTestInvisible`; `H2_RootSelfHitTestInvisibleAllowsInteractiveDescendants` PASS)
- `gp.UI.RunProductionHUDFoundationContractTest` — **Complete Failures=0 Cancelled=false**
- `gp.UI.RunLaunchMenuPresentationContractTest` — **Complete Failures=0 Cancelled=false**
- `gp.Resource.RunContainerLaunchContractTest` — **Complete Failures=0 Cancelled=false**, `SuiteComplete`
- `gp.Resource.RunContainerLaunchHUDContractTest` — **Complete Failures=0 Cancelled=None** (`None` = not cancelled), `SuiteComplete`

GP Development / Shipping not run.

## GPEditor / UHT result

`GPEditor Win64 Development` **Result: Succeeded** (UHT compiled-in). Adaptive rebuild compiled `GPHUDViewModelSubsystem.cpp` and `GPHUDBootstrapContractTest.cpp`.

## Exact changed files

- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDBootstrapContractTest.cpp`
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
- `WBP_GP_HUD`
- `WBP_GP_LaunchContainerRow`

Operator-authored Blueprint work remains local.

## TEMP HUD remains retired

No restore of TEMP HUD. Production HUD remains the active match HUD.

## No gameplay / economy semantic changes

Storage, orbital transfer, economy values, and container launch gameplay are unchanged.
