# Cursor Work Report — Production HUD ViewModel Bridge

## Status

**HUD_VIEWMODEL_BRIDGE_READY_FOR_OPERATOR_VALIDATION**

## Branch / base / head

- Branch: `feature/gp-production-hud-viewmodel-bridge`
- Base: `origin/main` @ `61cedc682a391225ac0a02a716f3d36a4c176d7e`
- Head: `e76a9bb89e38f0d08c1c950e7217bc53a92e8fc9`
- **NOT MERGED**
- **NOT FINALIZED**

## Factual UE 5.8 MVVM API used

Inspected installed UE 5.8.1 `ModelViewViewModel` headers:

- `UMVVMSubsystem::GetViewFromUserWidget(const UUserWidget*)` returns the widget's `UMVVMView` extension (`UserWidget->GetExtension<UMVVMView>()`).
- `UMVVMView::SetViewModel(FName ViewModelName, TScriptInterface<INotifyFieldValueChanged> ViewModel)` assigns a Manual/settable source by authored slot name and re-executes bindings if the view is already initialized.

No duplicate ViewModel objects are created by this path.

## Widget lifecycle hook and why

`UGP_HUDRootWidget::NativeConstruct` **after** `Super::NativeConstruct()`.

Why:

- `UUserWidget::NativeOnInitialized` runs `UMVVMViewClass::Initialize`, which **adds** `UMVVMView` (`ConstructView`).
- `UUserWidget::NativeConstruct` then constructs class extensions and calls `UMVVMView::Construct()`, which `InitializeSources` / `InitializeBindings` for Manual entries (no instance until set).
- Assigning immediately after Super uses the constructed view so `SetViewModel` can attach the subsystem instances and re-run bindings.
- No Tick. No timer retry. No world scan.

## ViewModel slot identifiers

Stable `FName` constants on `FGP_HUDViewModelBridge`:

- `GP_ResourceViewModel`
- `GP_MatchViewModel`

These match the authored WBP Manual MVVM entry display names. Existing ViewModel classes were not renamed.

## Ownership confirmation

`UGP_HUDViewModelSubsystem` remains the Outer/lifetime owner of ResourceVM, MatchVM, and adapters.
`UGP_HUDRootWidget` only assigns those pointers into the MVVM View.

## No duplicate VM creation

The bridge never `NewObject`s ResourceVM/MatchVM. Null subsystem VMs abort without replacements.

## Failure behavior

Safe no-crash returns with non-shipping warnings:

- no LocalPlayer
- no HUDViewModelSubsystem
- no `UMVVMView`
- `SetViewModel` false (missing/unconstructed slot)

## Tests / results

Headless full WBP Manual-slot success is not claimed (operator-authored `WBP_GP_HUD` is local and unstaged).
The contract proves fail-safe helper behavior, slot names, and subsystem ownership of existing instances.

| Command | Result |
| --- | --- |
| `gp.UI.RunProductionHUDFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |

## GPEditor / UHT

`Build.bat GPEditor Win64 Development` — **PASS** (UHT processed GPEditor; compile/link succeeded).

GP Win64 Development / Shipping **not run** (post-operator finalization).

## Exact changed files

- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDViewModelBridge.h` (new)
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDViewModelBridge.cpp` (new)
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDViewModelBridgeContractTest.cpp` (new)
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected Content confirmation

No `GP/Content/**`, Config, maps, Blueprints, DataAssets, or Tools files were modified or staged.
Operator-authored `WBP_GP_HUD` was not touched.

## NOT MERGED

## NOT FINALIZED
