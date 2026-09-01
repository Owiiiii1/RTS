# Cursor Work Report

## Status

**BOTTOM_HUD_PURCHASE_EXECUTION_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `0adab58ed826f2904c37385b230719f021da5d01`
- Previous authored Logistics Hub classification: `335b201c1a14e97590f1f177beeba8decd9b6cd3`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact reused RPCs

No new RPCs. HUD / presenter call existing local PlayerController wrappers, which still send the same server RPCs:

| HUD / presenter API | Existing PC wrapper | Existing RPC |
| --- | --- | --- |
| `RequestLaunchUnitShuttle()` | `RequestUnitDrop(PendingUnitManifest)` | `Server_RequestUnitDrop` |
| `RequestLaunchSelectedPurchaseItem()` Building / DefensiveBuilding | `RequestBuildingPurchase(ItemId)` | `Server_RequestBuildingPurchase` |
| Placement confirm (unchanged) | `RequestBuildingDeploy` / `ConfirmBuildingPlacement` | `Server_RequestBuildingDeploy` |
| `RequestLaunchSelectedPurchaseItem()` WallPackage | `RequestWallPackagePurchase()` | `Server_RequestWallPackagePurchase` |

Placement enter reuses `EnterBuildingPlacementMode(ItemId)`. Cancel reuses `CancelBuildingPlacement`.

## Unit manifest state and gates

Presenter owns local `FGP_UnitDropManifest PendingUnitManifest`. Row clicks do not RPC.

- `RequestPurchaseRowPrimary` in PurchaseUnits: +1 Worker or SalvageWalker if local gates pass
- `RequestPurchaseRowSecondary`: −1, min 0
- Quantity on `FGP_PurchaseCatalogRow.Quantity`
- Gates use `GPUnitDropAuthority::ComputeManifestCosts` (now `GPRUNTIME_API` for the UI module), `UGP_OrbitalDeliverySettings::PodTransportSlotCapacity`, local OrbitalFerronite, and `AGP_PlayerState::CanAcceptManifestUnitCount`
- Reject reasons stored as purchase contextual text: Shuttle capacity reached / Not enough Orbital Ferronite / Unit cap reached
- `GetPurchaseUnitManifestPresentation()`: WorkerCount, SalvageWalkerCount, UnitCount, UsedSlots, MaxSlots, TotalCost, bCanLaunch, DisabledReason

## Message strip state

`UGP_HUDRootWidget::GetContextMessage()`:

1. Command targeting prompt wins while Move / AttackMove / Patrol targeting is active (`GetCommandTargetingPrompt()` unchanged)
2. Else PurchaseUnits: empty manifest `Shuttle capacity: 0 / X slots`; non-empty `Shuttle: N / X slots`; or last local add-reject reason
3. Else empty

No global toast system.

## Selected-item states

Enum extended with `PurchaseBuildingSelected` and `PurchaseDefenseSelected`. No separate Wall state.

- Buildings LMB → `SelectedPurchaseItemId` + PurchaseBuildingSelected
- Defense DefensiveBuilding or WallPackage LMB → same for PurchaseDefenseSelected
- Select does not spend
- Building/Defense RMB: no-op
- `GetSelectedPurchaseItem()` returns identity row (ItemId, kind, name, icon, cost, enabled/reason, metadata). Empty catalog rows in selected states
- Selection away from friendly MainBase clears selected item, pending manifest, pending auto-deploy, and returns to Actions
- Back: PurchaseBuildingSelected → PurchaseBuildings; PurchaseDefenseSelected → PurchaseDefense; category Back → PurchaseRoot

## Building Purchase → READY → auto-placement sequencing

Factual: `Server_RequestBuildingPurchase` has **no client success ack**. READY is not faked.

1. Launch sets `PendingAutoDeployItemId` and snapshots `GetReadyCount` as `PendingAutoDeployReadyBefore`
2. Calls existing `RequestBuildingPurchase(ItemId)`
3. Server spends once and READY++
4. Presenter binds PlayerState `UGP_OrbitalBuildingInventoryComponent::OnReadyChanged`
5. If ItemId matches and `NewCount > ReadyBefore`, clear pending auto-deploy and call existing `EnterBuildingPlacementMode` (requires READY > 0)
6. Cancel placement: ghost exits, READY retained
7. Confirm: existing `Server_RequestBuildingDeploy` only. No second purchase/spend. No new spawn RPC

Pre-existing READY for the same ItemId does not auto-enter placement (`NewCount > ReadyBefore`).

## WallPackage flow

Selected WallPackage Launch → `RequestWallPackagePurchase()` only. No building purchase RPC. No READY. No placement mode. Returns to PurchaseDefense. Full stock / pending delivery disables selected launch (`bEnabled` false).

## No double spend proof / tests

- Building select / Back: OrbitalFerronite unchanged
- Launch increments presenter `DebugBuildingPurchaseRequestCount` once and READY +1
- Cancel keeps READY; `ConfirmBuildingPlacement` while inactive does not increment purchase count
- Wall Launch increments wall request count only; building purchase count and READY totals unchanged
- Unit Launch clears pending manifest immediately (no client success seam exists; avoids duplicate launch from stale manifest). Documented limitation: submit is optimistic; server remains final authority / logs

Focused contract: `gp.UI.RunPurchaseExecutionContractTest`

## Blueprint API

HUD root (do not modify WBP):

- `RequestPurchaseRowPrimary(FPrimaryAssetId)`
- `RequestPurchaseRowSecondary(FPrimaryAssetId)`
- `RequestLaunchUnitShuttle()`
- `RequestLaunchSelectedPurchaseItem()`
- `GetSelectedPurchaseItem()`
- `GetPurchaseUnitManifestPresentation()`
- `GetContextMessage()`
- existing `RequestPurchaseBack()` / catalog getters / panel state

`GetCommandTargetingPrompt()` remains targeting-only.

## Exact operator wiring

Protected WBP remain operator-owned. Minimal wiring:

- `WBP_GP_PurchaseRow` `BTN_Row` OnClicked → `RequestPurchaseRowPrimary(RowData.ItemId)`
- Row RMB: Blueprint `OnMouseButtonDown` RightMouseButton → `RequestPurchaseRowSecondary(RowData.ItemId)` (Button OnClicked cannot distinguish RMB)
- Quantity text from `RowData.Quantity`
- Units `BTN_LaunchShuttle` → `RequestLaunchUnitShuttle()`
- Selected overlay (operator may add later): `BTN_SelectedLaunch` → `RequestLaunchSelectedPurchaseItem()`; `BTN_SelectedBack` → `RequestPurchaseBack()`

## Tests

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

Headless `-game -nullrhi -unattended -nop4` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunPurchaseExecutionContractTest` | Complete Failures=0 |
| `gp.UI.RunPurchaseCatalogPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Orbital.RunWallPackageInventoryContractTest` | Complete Failures=0 |
| `gp.Building.RunBuildGridContractTest` | Complete Failures=0 |
| `gp.Building.RunMultiBuildingDataContractTest` | Complete Failures=0 |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | Complete Failures=0 |
| `gp.Commands.RunMovePatrolTargetingContractTest` | Complete Failures=0 |

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseExecutionContractTest.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPUnitDropAuthority.h` (`ComputeManifestCosts` export only)
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `Content/`, authored DataAssets, `Config/`, maps, Materials, VFX, `Tools/`, `GP.uproject`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator note

Wire row LMB/RMB, Launch Shuttle, and selected-item Launch/Back to the HUD root APIs above. Authored selected-item overlays/pages may still be added locally. INTERMEDIATE / NOT MERGE READY.
