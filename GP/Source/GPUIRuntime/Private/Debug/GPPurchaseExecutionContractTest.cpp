// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "UObject/Package.h"
#include "ViewModels/GPContextActionPresenter.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPurchaseExecutionContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPPurchaseExecutionContractPrivate
{
	static const FGP_PurchaseCatalogRow* FindRowById(
		const TArray<FGP_PurchaseCatalogRow>& Rows,
		const FPrimaryAssetId& ItemId)
	{
		for (const FGP_PurchaseCatalogRow& Row : Rows)
		{
			if (Row.ItemId == ItemId)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	static void SetAttr(AGP_PlayerState* PlayerState, const FGameplayAttribute& Attr, float Value)
	{
		if (!IsValid(PlayerState))
		{
			return;
		}
		if (UGP_AbilitySystemComponent* ASC = PlayerState->GetGPAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(Attr, Value);
		}
	}

	template <typename TActor>
	static TActor* SpawnOwned(UWorld* World, const FVector& Location, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		TActor* Actor = World->SpawnActor<TActor>(
			TActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (Actor != nullptr)
		{
			Actor->SetTeamId(TeamId);
		}
		return Actor;
	}

	static void RunPurchaseExecutionContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPPurchaseExecutionContract, Warning,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPPurchaseExecutionContract, Log,
					TEXT("gp.UI.RunPurchaseExecutionContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPPurchaseExecutionContract, Error,
					TEXT("gp.UI.RunPurchaseExecutionContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestPurchaseRowPrimary")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestPurchaseRowSecondary")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestLaunchUnitShuttle")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestLaunchSelectedPurchaseItem")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetSelectedPurchaseItem")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetPurchaseUnitManifestPresentation")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetContextMessage")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("Server_RequestUnitDrop")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("Server_RequestBuildingPurchase")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("Server_RequestBuildingDeploy")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("Server_RequestWallPackagePurchase")) != nullptr,
			TEXT("A0_HUDAndPCExposeExistingPurchaseSeams"));

		UGP_OrbitalUnitDropCatalog& UnitCatalog = UGP_OrbitalUnitDropCatalog::Get();
		UGP_BuildingDropCatalog& BuildingCatalog = UGP_BuildingDropCatalog::Get();
		UGP_WallPackageCatalog* WallCatalog = UGP_WallPackageCatalog::Get();
		UnitCatalog.DebugBeginContractIsolation();
		BuildingCatalog.DebugBeginContractIsolation();
		if (WallCatalog != nullptr)
		{
			WallCatalog->DebugBeginContractIsolation();
		}

		auto EndIsolation = [&]()
		{
			UnitCatalog.DebugClearAuthoredUnitDropOverrides();
			BuildingCatalog.DebugClearAuthoredBuildingDropOverrides();
			if (WallCatalog != nullptr)
			{
				WallCatalog->DebugClearAuthoredOverrides();
				WallCatalog->DebugEndContractIsolation();
			}
			BuildingCatalog.DebugEndContractIsolation();
			UnitCatalog.DebugEndContractIsolation();
		};

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		AGP_PlayerController* PlayerController =
			LocalPlayer != nullptr
				? Cast<AGP_PlayerController>(LocalPlayer->GetPlayerController(World))
				: Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		UGP_SelectionComponent* Selection =
			PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
		UGP_ContextActionPresenter* Presenter =
			Subsystem != nullptr ? Subsystem->GetContextActionPresenter() : nullptr;
		AGP_PlayerState* PlayerState =
			PlayerController != nullptr ? PlayerController->GetPlayerState<AGP_PlayerState>() : nullptr;

		Expect(IsValid(PlayerController) && IsValid(Selection) && IsValid(Presenter)
			&& IsValid(PlayerState),
			TEXT("A1_LocalPresenterAndPlayerState"));
		if (!IsValid(PlayerController) || !IsValid(Selection) || !IsValid(Presenter)
			|| !IsValid(PlayerState))
		{
			EndIsolation();
			UE_LOG(LogGPPurchaseExecutionContract, Log,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		int32 LocalTeamId = PlayerState->GetTeamId();
		if (LocalTeamId < 1)
		{
			LocalTeamId = 1;
		}

		AGP_MainBase* MainBase = SpawnOwned<AGP_MainBase>(
			World, FVector(-53000.0f, -15000.0f, 100.0f), LocalTeamId);
		Expect(IsValid(MainBase), TEXT("A2_SpawnedMainBase"));
		if (!IsValid(MainBase))
		{
			EndIsolation();
			UE_LOG(LogGPPurchaseExecutionContract, Log,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitCatalog.GetWorkerDrop();
		UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitCatalog.GetSalvageWalkerDrop();
		UGP_OrbitalDropDefinition* HubDrop = BuildingCatalog.GetLegacyLogisticsHubDrop();
		UGP_OrbitalDropDefinition* TurretDrop = BuildingCatalog.DebugGetCanonicalDefensiveTurretDrop();
		UGP_WallPackageDefinition* WallPackage =
			WallCatalog != nullptr ? WallCatalog->GetWallPackage() : nullptr;
		Expect(IsValid(WorkerDrop) && IsValid(WalkerDrop) && IsValid(HubDrop)
			&& IsValid(TurretDrop) && IsValid(WallPackage),
			TEXT("A3_CanonicalProducts"));
		if (!IsValid(WorkerDrop) || !IsValid(WalkerDrop) || !IsValid(HubDrop)
			|| !IsValid(TurretDrop) || !IsValid(WallPackage))
		{
			MainBase->Destroy();
			EndIsolation();
			UE_LOG(LogGPPurchaseExecutionContract, Log,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetMaxUnitsAttribute(), 10.0f);
		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetCurrentUnitsAttribute(), 0.0f);
		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute(), 10000.0f);

		Selection->ReplaceSelectionWithUnit(MainBase);
		Presenter->RequestOpenMainBasePurchase();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);

		FGP_PurchaseUnitManifestPresentation Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.UnitCount == 0 && Manifest.UsedSlots == 0 && !Manifest.bCanLaunch
			&& Presenter->GetContextMessage().ToString().Contains(TEXT("Shuttle capacity:")),
			TEXT("UnitA_EmptyManifest"));

		Presenter->RequestLaunchUnitShuttle();
		Expect(Presenter->DebugGetUnitDropRequestCount() == 0, TEXT("UnitH_LaunchEmptyBlocked"));

		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		const FGP_PurchaseCatalogRow* WorkerQty1 =
			FindRowById(Presenter->GetPurchaseCatalogRows(), WorkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(WorkerQty1 != nullptr && WorkerQty1->Quantity == 1
			&& Manifest.WorkerCount == 1 && Manifest.UsedSlots == WorkerDrop->TransportSlotCost
			&& FMath::IsNearlyEqual(Manifest.TotalCost, WorkerDrop->Cost)
			&& Manifest.bCanLaunch,
			TEXT("UnitB_LMBWorkerQty1"));

		Presenter->RequestPurchaseRowPrimary(WalkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.SalvageWalkerCount == 1
			&& Manifest.UsedSlots == WorkerDrop->TransportSlotCost + WalkerDrop->TransportSlotCost,
			TEXT("UnitC_LMBWalkerIncrements"));

		Presenter->RequestPurchaseRowSecondary(WalkerDrop->GetPrimaryAssetId());
		Presenter->RequestPurchaseRowSecondary(WalkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.SalvageWalkerCount == 0, TEXT("UnitD_RMBNeverBelowZero"));

		Presenter->RequestPurchaseRowSecondary(WorkerDrop->GetPrimaryAssetId());
		const int32 Capacity = UGP_OrbitalDeliverySettings::Get() != nullptr
			? FMath::Max(1, UGP_OrbitalDeliverySettings::Get()->PodTransportSlotCapacity)
			: 4;
		for (int32 i = 0; i < Capacity; ++i)
		{
			Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		}
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		const int32 SlotsAtCap = Manifest.UsedSlots;
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.UsedSlots == SlotsAtCap
			&& Presenter->GetContextMessage().ToString().Contains(TEXT("Shuttle capacity reached")),
			TEXT("UnitE_SlotOverflowBlockedLocally"));

		while (Presenter->GetPurchaseUnitManifestPresentation().WorkerCount > 0)
		{
			Presenter->RequestPurchaseRowSecondary(WorkerDrop->GetPrimaryAssetId());
		}
		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute(), WorkerDrop->Cost);
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.WorkerCount == 1
			&& Presenter->GetContextMessage().ToString().Contains(TEXT("Orbital")),
			TEXT("UnitF_FundsOverflowBlockedLocally"));

		Presenter->RequestPurchaseRowSecondary(WorkerDrop->GetPrimaryAssetId());
		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute(), 10000.0f);
		const int32 MaxUnitsNow = FMath::Max(1,
			FMath::RoundToInt(PlayerState->GetPlayerAttributeSet()->GetMaxUnits()));
		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetCurrentUnitsAttribute(),
			static_cast<float>(MaxUnitsNow - 1));
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		Manifest = Presenter->GetPurchaseUnitManifestPresentation();
		Expect(Manifest.WorkerCount == 1
			&& Presenter->GetContextMessage().ToString().Contains(TEXT("Unit cap")),
			TEXT("UnitG_UnitCapBlockedLocally"));

		SetAttr(PlayerState, UGP_PlayerAttributeSet::GetCurrentUnitsAttribute(), 0.0f);
		while (Presenter->GetPurchaseUnitManifestPresentation().WorkerCount > 0)
		{
			Presenter->RequestPurchaseRowSecondary(WorkerDrop->GetPrimaryAssetId());
		}
		Presenter->RequestPurchaseRowPrimary(WorkerDrop->GetPrimaryAssetId());
		const int32 DropRequestsBefore = Presenter->DebugGetUnitDropRequestCount();
		Presenter->RequestLaunchUnitShuttle();
		Expect(Presenter->DebugGetUnitDropRequestCount() == DropRequestsBefore + 1
			&& Presenter->DebugGetLastRequestedUnitManifest().WorkerCount == 1
			&& Presenter->GetPurchaseUnitManifestPresentation().UnitCount == 0,
			TEXT("UnitI_LaunchRoutesExistingUnitDrop"));
		Presenter->RequestLaunchUnitShuttle();
		Expect(Presenter->DebugGetUnitDropRequestCount() == DropRequestsBefore + 1,
			TEXT("UnitJ_NoDuplicateLaunchFromClearedManifest"));

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
		const UGP_PlayerAttributeSet* PlayerAttrs = PlayerState->GetPlayerAttributeSet();
		Expect(PlayerAttrs != nullptr, TEXT("Building_PlayerAttributes"));
		if (PlayerAttrs == nullptr)
		{
			Selection->ClearAllSelectionState();
			if (IsValid(MainBase))
			{
				MainBase->Destroy();
			}
			EndIsolation();
			UE_LOG(LogGPPurchaseExecutionContract, Log,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}
		const float OrbitalBeforeSelect = PlayerAttrs->GetOrbitalFerronite();
		Presenter->RequestPurchaseRowPrimary(HubDrop->GetPrimaryAssetId());
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseBuildingSelected
			&& Presenter->GetSelectedPurchaseItem().ItemId == HubDrop->GetPrimaryAssetId()
			&& Presenter->GetPurchaseCatalogRows().Num() == 0
			&& FMath::IsNearlyEqual(PlayerAttrs->GetOrbitalFerronite(), OrbitalBeforeSelect),
			TEXT("BuildingA_SelectNoSpend"));

		Presenter->RequestPurchaseBack();
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseBuildings
			&& !Presenter->GetSelectedPurchaseItem().ItemId.IsValid()
			&& FMath::IsNearlyEqual(PlayerAttrs->GetOrbitalFerronite(), OrbitalBeforeSelect),
			TEXT("BuildingB_BackNoSpend"));

		Presenter->RequestPurchaseRowPrimary(FPrimaryAssetId());
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseBuildings,
			TEXT("BuildingG_InvalidItemNoOp"));

		Presenter->RequestPurchaseRowPrimary(HubDrop->GetPrimaryAssetId());
		UGP_OrbitalBuildingInventoryComponent* Inventory =
			PlayerState->GetOrbitalBuildingInventoryComponent();
		Expect(Inventory != nullptr, TEXT("Building_ReadyInventory"));
		if (Inventory == nullptr)
		{
			Selection->ClearAllSelectionState();
			if (IsValid(MainBase))
			{
				MainBase->Destroy();
			}
			EndIsolation();
			UE_LOG(LogGPPurchaseExecutionContract, Log,
				TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}
		const int32 ReadyBefore = Inventory->GetReadyCount(HubDrop);
		const int32 PurchaseRequestsBefore = Presenter->DebugGetBuildingPurchaseRequestCount();
		Presenter->RequestLaunchSelectedPurchaseItem();
		Expect(Presenter->DebugGetBuildingPurchaseRequestCount() == PurchaseRequestsBefore + 1
			&& Presenter->DebugGetLastRequestedBuildingPurchaseId() == HubDrop->GetPrimaryAssetId(),
			TEXT("BuildingC_LaunchUsesExistingPurchaseRequest"));
		Expect(Inventory != nullptr && Inventory->GetReadyCount(HubDrop) == ReadyBefore + 1
			&& PlayerController->IsBuildingPlacementActive(),
			TEXT("BuildingD_ReadyIncrementEntersPlacement"));

		PlayerController->CancelBuildingPlacement();
		Expect(!PlayerController->IsBuildingPlacementActive()
			&& Inventory->GetReadyCount(HubDrop) == ReadyBefore + 1,
			TEXT("BuildingE_CancelKeepsReady"));

		const int32 PurchaseAfterCancel = Presenter->DebugGetBuildingPurchaseRequestCount();
		PlayerController->ConfirmBuildingPlacement();
		Expect(Presenter->DebugGetBuildingPurchaseRequestCount() == PurchaseAfterCancel
			&& Inventory->GetReadyCount(HubDrop) == ReadyBefore + 1,
			TEXT("BuildingF_ConfirmWithoutPlacementDoesNotPurchaseAgain"));

		Selection->ReplaceSelectionWithUnit(MainBase);
		Presenter->RequestOpenMainBasePurchase();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Defense);
		Presenter->RequestPurchaseRowPrimary(TurretDrop->GetPrimaryAssetId());
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseDefenseSelected
			&& Presenter->GetSelectedPurchaseItem().ItemKind == EGP_PurchaseCatalogItemKind::DefensiveBuilding,
			TEXT("DefenseA_TurretSelects"));
		const int32 TurretReadyBefore = Inventory->GetReadyCount(TurretDrop);
		const int32 TurretPurchaseBefore = Presenter->DebugGetBuildingPurchaseRequestCount();
		Presenter->RequestLaunchSelectedPurchaseItem();
		Expect(Presenter->DebugGetBuildingPurchaseRequestCount() == TurretPurchaseBefore + 1
			&& Inventory->GetReadyCount(TurretDrop) == TurretReadyBefore + 1
			&& PlayerController->IsBuildingPlacementActive(),
			TEXT("DefenseA_TurretLaunchPurchaseAndAutoDeploy"));
		PlayerController->CancelBuildingPlacement();

		Selection->ReplaceSelectionWithUnit(MainBase);
		Presenter->RequestOpenMainBasePurchase();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Defense);
		Presenter->RequestPurchaseRowPrimary(WallPackage->GetPrimaryAssetId());
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseDefenseSelected
			&& Presenter->GetSelectedPurchaseItem().ItemKind == EGP_PurchaseCatalogItemKind::WallPackage,
			TEXT("WallB_PackageSelects"));
		const int32 WallRequestsBefore = Presenter->DebugGetWallPackagePurchaseRequestCount();
		const int32 BuildingRequestsBeforeWall = Presenter->DebugGetBuildingPurchaseRequestCount();
		const int32 ReadyTotalBeforeWall = Inventory->GetReadyCount(HubDrop) + Inventory->GetReadyCount(TurretDrop);
		Presenter->RequestLaunchSelectedPurchaseItem();
		Expect(Presenter->DebugGetWallPackagePurchaseRequestCount() == WallRequestsBefore + 1
			&& Presenter->DebugGetBuildingPurchaseRequestCount() == BuildingRequestsBeforeWall
			&& !PlayerController->IsBuildingPlacementActive()
			&& Inventory->GetReadyCount(HubDrop) + Inventory->GetReadyCount(TurretDrop) == ReadyTotalBeforeWall
			&& Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseDefense,
			TEXT("WallBC_PackageUsesWallPurchaseNoReadyNoPlacement"));

		Presenter->RequestPurchaseRowPrimary(WallPackage->GetPrimaryAssetId());
		if (UGP_WallSegmentInventoryComponent* WallInv = MainBase->GetWallSegmentInventoryComponent())
		{
			WallInv->DebugForceSetStock(UGP_WallSegmentInventoryComponent::DefaultCapacity);
			Expect(!Presenter->GetSelectedPurchaseItem().bEnabled,
				TEXT("WallD_FullStockDisablesSelectedLaunch"));
			const int32 WallAfterDisable = Presenter->DebugGetWallPackagePurchaseRequestCount();
			Presenter->RequestLaunchSelectedPurchaseItem();
			Expect(Presenter->DebugGetWallPackagePurchaseRequestCount() == WallAfterDisable,
				TEXT("WallD_DisabledLaunchNoOp"));
			WallInv->DebugForceSetStock(0);
		}

		Selection->ClearAllSelectionState();
		if (IsValid(MainBase))
		{
			MainBase->Destroy();
		}
		EndIsolation();
		UE_LOG(LogGPPurchaseExecutionContract, Log,
			TEXT("gp.UI.RunPurchaseExecutionContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GPurchaseExecutionContract(
		TEXT("gp.UI.RunPurchaseExecutionContractTest"),
		TEXT("Run production HUD Purchase execution / manifest / launch contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunPurchaseExecutionContractTest));
}

#endif
