# Cursor Work Report — TEMP_S28P HUD Removal

## Status

**TEMP_HUD_REMOVAL_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

## Branch / base / head

- Branch: `cleanup/gp-remove-temp-hud`
- Base: `origin/main` @ `7fc2ab9ee880e73e1c7610570cc9a723b9376905`
- Head: this implementation commit on `cleanup/gp-remove-temp-hud` (pushed with this report)

## Exact deleted files

- `GP/Source/GPRuntime/Public/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`

There is no remaining runtime creation of `UGP_TEMP_S28P_PlanetaryFerroniteHUD`.

## PlayerController TEMP presentation code removed

Class A only (TEMP presentation scaffolding). Removed from `AGP_PlayerController`:

Lifecycle / bind / sync:

- `EnsurePlanetaryFerroniteHUD`
- `DestroyPlanetaryFerroniteHUD`
- `RefreshPlanetaryFerroniteHUDBinding`
- `ClearPlanetaryFerroniteHUDBindings`
- `BindPlanetaryFerroniteStorage`
- `UnbindPlanetaryFerroniteStorage`
- `SyncPlanetaryFerroniteHUDFromStorage`
- `SyncLaunchButtonFromStorage`
- `BindOrbitalFerroniteAttribute`
- `UnbindOrbitalFerroniteAttribute`
- `SyncOrbitalFerroniteHUDFromAttributes`
- `SyncBuildingReadyHUDFromInventory`
- `BindWallInventoryEvents`
- `UnbindWallInventoryEvents`
- `SyncWallPackageHUDFromInventory`
- `BindBuildingInventoryEvents`
- `UnbindBuildingInventoryEvents`

TEMP-HUD-specific handlers:

- `HandleWallInventoryChangedForHUD`
- `HandleWallPackagePendingChangedForHUD`
- `HandleOrbitalFerroniteAttributeChanged`
- `HandleMaxUnitsAttributeChanged`
- `HandleCurrentUnitsAttributeChanged`
- `HandleFerroniteScoreAttributeChanged`
- `SyncUnitCapHUDFromAttributes`
- `SyncMatchHUDFromAuthority`
- `HandleMatchTimeRemainingChanged`
- `HandleMatchStateTagChanged`
- `HandleMatchResultChangedForHUD`
- `HandleBuildingReadyChanged`
- `HandleResolvedMainBaseChanged`
- `HandlePlayerTeamIdChanged`
- `HandleStorageChangedForHUD`

TEMP presentation members:

- `PlanetaryFerroniteHUD`
- `BoundPlanetaryGameState`
- `BoundPlanetaryPlayerState`
- `BoundPlanetaryStorage`
- `BoundWallInventory`
- `BoundOrbitalASC`
- `ResolvedMainBaseChangedHandle`
- `PlayerTeamIdChangedHandle`
- `OrbitalFerroniteChangedHandle`
- `MaxUnitsChangedHandle`
- `CurrentUnitsChangedHandle`
- `FerroniteScoreChangedHandle`
- `BuildingReadyChangedHandle`
- `MatchTimeRemainingChangedHandle`
- `MatchStateTagChangedHandle`
- `MatchResultChangedHandle`
- `BoundPlanetaryTeamId`
- `PlanetaryFerroniteHUDZOrder`

TEMP HUD create/destroy calls removed from PlayerController lifecycle (`BeginPlayingState` / PlayerState ready / ASC link / `EndPlay`). `Client_NotifyUnitDropRejected` remains; it no longer talks to TEMP HUD.

`AGP_PlayerController` no longer owns TEMP HUD presentation state.

## Gameplay / RPC APIs explicitly preserved

Class B — still on `AGP_PlayerController`:

- `RequestLaunchReadyContainer` / `Server_RequestLaunchReadyContainer` / `AuthorityTryLaunchReadyContainerForOwningTeam`
- `RequestUnitDrop` / `Server_RequestUnitDrop` / `AuthorityTryRequestUnitDrop`
- `RequestBuildingPurchase` / `Server_RequestBuildingPurchase` / `AuthorityTryPurchaseBuilding`
- `RequestWallPackagePurchase` / `Server_RequestWallPackagePurchase` / `AuthorityTryPurchaseWallPackage`
- Building deploy / placement: `RequestBuildingDeploy` / `Server_RequestBuildingDeploy` / `AuthorityTryDeployBuilding` / `CancelBuildingPlacement` / `ConfirmBuildingPlacement`

Storage, orbital inventory, GAS attributes, GameState match delegates, and building/wall procurement rules were not changed.

## Production HUD ownership / bootstrap unchanged

Not touched:

- `GP/Content/**` (including operator-local `/Game/GrimProtocol/Blueprint/Widgets/WBP_GP_HUD`)
- Production HUD bindings / authored layout

Canonical bootstrap remains:

`UGP_HUDViewModelSubsystem` → configured `ProductionHUDWidgetClass` → `WBP_GP_HUD`

No replacement C++ widget. Production HUD ownership was not moved into PlayerController.

## Contracts / regressions run

Anti-reintroduction checks:

- `K_TEMPHUDRetiredNotOwnedByPlayerController` (`FindObject` `/Script/GPRuntime.GP_TEMP_S28P_PlanetaryFerroniteHUD` == null; no `PlanetaryFerroniteHUD` property on PC)
- `D_TEMPHUDPathRemovedFromPlayerController`
- `M_GameplayRequestRPCsRemain` (`Server_RequestLaunchReadyContainer`, `Server_RequestUnitDrop`, `Server_RequestBuildingPurchase`, `Server_RequestWallPackagePurchase`)

UI (all `Complete Failures=0`):

- `gp.UI.RunProductionHUDFoundationContractTest`
- `gp.UI.RunHUDViewModelBridgeContractTest`
- `gp.UI.RunHUDBootstrapContractTest`
- `gp.UI.RunPlanetFerronitePresentationContractTest`
- `gp.UI.RunThreatPresentationContractTest`

Gameplay (all `Complete Failures=0`; rewritten launch HUD contract is gameplay-only, no TEMP widget):

- `gp.Resource.RunContainerLaunchContractTest`
- `gp.Resource.RunContainerLaunchHUDContractTest` (`PASS: SuiteComplete`, including `A_LaunchRPCRemains`)
- `gp.Resource.RunOrbitalUnitDropContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Orbital.RunWallPackageInventoryContractTest`

Full suite not run. GP Development / Shipping not run.

## GPEditor / UHT result

`GPEditor Win64 Development` + UHT: **PASS** (`Result: Succeeded`).

GP Development / Shipping: **not run** (post-operator finalization only).

## Exact changed files

Deleted:

- `GP/Source/GPRuntime/Public/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`

Source:

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Resources/GPStorageComponent.h` (comment only: TEMP HUD retired)
- `GP/Source/GPRuntime/Private/Debug/GPContainerLaunchHUDContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMatchWinLoseContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDBootstrapContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPProductionHUDFoundationContractTest.cpp`

Docs (current-state only; historical task docs not rewritten):

- `Docs/TDD/12_UI_Architecture.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md` (this report)

## Content / protected files untouched

Not touched and not staged:

- `GP/Content/**`
- `GP/Config/**`
- maps
- DataAssets
- Tools

Local dirty authored work left unstaged.

## No gameplay / economy semantic changes

Procurement/request APIs remain available for future production context-action UI. HUD ViewModels / adapters continue functioning. No duplicate top HUD from TEMP widget instantiation.

## Merge / finalization

**NOT MERGED.**

**NOT FINALIZED.**
