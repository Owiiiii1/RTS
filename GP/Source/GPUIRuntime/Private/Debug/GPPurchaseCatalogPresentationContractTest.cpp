// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/Package.h"
#include "Units/GPWorker.h"
#include "ViewModels/GPContextActionPresenter.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPurchaseCatalogPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPPurchaseCatalogPresentationContractPrivate
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

	static bool HasKind(
		const TArray<FGP_PurchaseCatalogRow>& Rows,
		EGP_PurchaseCatalogItemKind ItemKind)
	{
		for (const FGP_PurchaseCatalogRow& Row : Rows)
		{
			if (Row.ItemKind == ItemKind)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasBuildingType(
		const TArray<FGP_PurchaseCatalogRow>& Rows,
		const FGameplayTag& BuildingTypeTag)
	{
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		for (const FGP_PurchaseCatalogRow& Row : Rows)
		{
			if (UGP_OrbitalDropDefinition* Drop = Catalog.FindDropDefinition(Row.ItemId))
			{
				if (const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition())
				{
					if (BuildingTypeTag.IsValid() && Building->BuildingTags.HasTagExact(BuildingTypeTag))
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	static bool HasWallPackage(const TArray<FGP_PurchaseCatalogRow>& Rows)
	{
		return HasKind(Rows, EGP_PurchaseCatalogItemKind::WallPackage);
	}

	static void SetOrbital(AGP_PlayerState* PlayerState, float Value)
	{
		if (!IsValid(PlayerState))
		{
			return;
		}
		if (UGP_AbilitySystemComponent* ASC = PlayerState->GetGPAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute(), Value);
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

	static void RunPurchaseCatalogPresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPPurchaseCatalogPresentationContract, Warning,
				TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPPurchaseCatalogPresentationContract, Log,
					TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPPurchaseCatalogPresentationContract, Error,
					TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_ContextActionPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_ContextActionPresenter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr,
			TEXT("A0_NoTickOnContextActionPresenter"));
		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetPurchaseCatalogRows")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("BP_OnContextActionsChanged")) != nullptr,
			TEXT("A0_HUDRootExposesPurchaseCatalogAPI"));

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
			UE_LOG(LogGPPurchaseCatalogPresentationContract, Log,
				TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		int32 LocalTeamId = PlayerState->GetTeamId();
		if (LocalTeamId < 1)
		{
			LocalTeamId = 1;
		}

		Selection->ClearAllSelectionState();
		Expect(Presenter->GetPurchaseCatalogRows().Num() == 0, TEXT("H_EmptyWhenNoSelection"));

		AGP_Worker* Worker = SpawnOwned<AGP_Worker>(
			World, FVector(-52000.0f, -14000.0f, 100.0f), LocalTeamId);
		AGP_MainBase* MainBase = SpawnOwned<AGP_MainBase>(
			World, FVector(-52100.0f, -13000.0f, 100.0f), LocalTeamId);
		Expect(IsValid(Worker) && IsValid(MainBase), TEXT("A2_SpawnedContractActors"));
		if (!IsValid(Worker) || !IsValid(MainBase))
		{
			EndIsolation();
			UE_LOG(LogGPPurchaseCatalogPresentationContract, Log,
				TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		auto DestroySpawned = [&]()
		{
			Selection->ClearAllSelectionState();
			if (IsValid(Worker)) { Worker->Destroy(); }
			if (IsValid(MainBase)) { MainBase->Destroy(); }
		};

		UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitCatalog.GetWorkerDrop();
		UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitCatalog.GetSalvageWalkerDrop();
		UGP_OrbitalDropDefinition* HubDrop = BuildingCatalog.GetLegacyLogisticsHubDrop();
		UGP_OrbitalDropDefinition* TurretDrop = BuildingCatalog.DebugGetCanonicalDefensiveTurretDrop();
		UGP_WallPackageDefinition* WallPackage =
			WallCatalog != nullptr ? WallCatalog->GetWallPackage() : nullptr;
		const FGPGameplayTags& Tags = FGPGameplayTags::Get();

		Expect(IsValid(WorkerDrop) && IsValid(WalkerDrop) && IsValid(HubDrop)
			&& IsValid(TurretDrop) && IsValid(WallPackage),
			TEXT("A3_CanonicalCatalogProductsReady"));
		if (!IsValid(WorkerDrop) || !IsValid(WalkerDrop) || !IsValid(HubDrop)
			|| !IsValid(TurretDrop) || !IsValid(WallPackage))
		{
			DestroySpawned();
			EndIsolation();
			UE_LOG(LogGPPurchaseCatalogPresentationContract, Log,
				TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		Selection->ReplaceSelectionWithUnit(Worker);
		Expect(Presenter->GetMode() != EGP_ContextActionMode::MainBase
			&& Presenter->GetPurchaseCatalogRows().Num() == 0,
			TEXT("H_NonMainBaseEmptyCatalog"));

		Selection->ReplaceSelectionWithUnit(MainBase);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::MainBase
			&& Presenter->GetPanelState() == EGP_ContextActionPanelState::Actions
			&& Presenter->GetPurchaseCatalogRows().Num() == 0,
			TEXT("H_ActionsEmptyCatalog"));

		Presenter->RequestOpenMainBasePurchase();
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseRoot
			&& Presenter->GetPurchaseCatalogRows().Num() == 0,
			TEXT("H_PurchaseRootEmptyCatalog"));

		SetOrbital(PlayerState, 0.0f);
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);
		const TArray<FGP_PurchaseCatalogRow>& UnitRowsLow = Presenter->GetPurchaseCatalogRows();
		const FGP_PurchaseCatalogRow* WorkerRowLow = FindRowById(UnitRowsLow, WorkerDrop->GetPrimaryAssetId());
		const FGP_PurchaseCatalogRow* WalkerRowLow = FindRowById(UnitRowsLow, WalkerDrop->GetPrimaryAssetId());
		Expect(WorkerRowLow != nullptr && WalkerRowLow != nullptr
			&& UnitRowsLow.Num() == 2,
			TEXT("A_UnitsWorkerAndSalvageWalkerPresent"));
		Expect(WorkerRowLow != nullptr
			&& WorkerRowLow->ItemKind == EGP_PurchaseCatalogItemKind::Unit
			&& WorkerRowLow->Category == EGP_PurchaseCategory::Units
			&& WorkerRowLow->DisplayName.EqualTo(WorkerDrop->DisplayName)
			&& FMath::IsNearlyEqual(WorkerRowLow->Cost, WorkerDrop->Cost)
			&& FMath::IsNearlyEqual(WorkerRowLow->Cost, UnitCatalog.GetWorkerOrbitalDropCost())
			&& WorkerRowLow->TransportSlotCost == WorkerDrop->TransportSlotCost
			&& WorkerRowLow->TransportSlotCost == UnitCatalog.GetWorkerTransportSlotCost(),
			TEXT("A_D_WorkerNameCostSlotFromCatalog"));
		Expect(WalkerRowLow != nullptr
			&& WalkerRowLow->ItemKind == EGP_PurchaseCatalogItemKind::Unit
			&& WalkerRowLow->Category == EGP_PurchaseCategory::Units
			&& WalkerRowLow->DisplayName.EqualTo(WalkerDrop->DisplayName)
			&& FMath::IsNearlyEqual(WalkerRowLow->Cost, WalkerDrop->Cost)
			&& FMath::IsNearlyEqual(WalkerRowLow->Cost, UnitCatalog.GetSalvageWalkerOrbitalDropCost())
			&& WalkerRowLow->TransportSlotCost == WalkerDrop->TransportSlotCost
			&& WalkerRowLow->TransportSlotCost == UnitCatalog.GetSalvageWalkerTransportSlotCost(),
			TEXT("A_D_WalkerNameCostSlotFromCatalog"));
		Expect(WorkerRowLow == nullptr || WorkerRowLow->Icon == nullptr,
			TEXT("E_NullIconAcceptedOnNativeWorker"));
		Expect(WorkerRowLow != nullptr
			&& !WorkerRowLow->bEnabled
			&& WorkerRowLow->DisabledReason.ToString().Contains(TEXT("Insufficient")),
			TEXT("F_InsufficientOrbitalDisablesWorker"));

		SetOrbital(PlayerState, 10000.0f);
		const TArray<FGP_PurchaseCatalogRow>& UnitRowsHigh = Presenter->GetPurchaseCatalogRows();
		const FGP_PurchaseCatalogRow* WorkerRowHigh = FindRowById(UnitRowsHigh, WorkerDrop->GetPrimaryAssetId());
		const FGP_PurchaseCatalogRow* WalkerRowHigh = FindRowById(UnitRowsHigh, WalkerDrop->GetPrimaryAssetId());
		Expect(WorkerRowHigh != nullptr && WorkerRowHigh->bEnabled
			&& WorkerRowHigh->DisabledReason.IsEmpty()
			&& WalkerRowHigh != nullptr && WalkerRowHigh->bEnabled,
			TEXT("G_SufficientOrbitalEnablesReadyUnits"));

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
		const TArray<FGP_PurchaseCatalogRow>& BuildingRows = Presenter->GetPurchaseCatalogRows();
		Expect(HasBuildingType(BuildingRows, Tags.Building_Type_LogisticsHub),
			TEXT("B_LogisticsHubPresentInBuildings"));
		Expect(!HasBuildingType(BuildingRows, Tags.Building_Type_DefensiveTurret),
			TEXT("B_DefensiveTurretAbsentFromBuildings"));
		Expect(!HasBuildingType(BuildingRows, Tags.Building_Type_MainBase),
			TEXT("B_MainBaseAbsentFromBuildings"));
		Expect(!HasWallPackage(BuildingRows), TEXT("B_WallPackageAbsentFromBuildings"));
		Expect(!HasKind(BuildingRows, EGP_PurchaseCatalogItemKind::Unit),
			TEXT("I_BuildingsHasNoUnitRows"));
		const FGP_PurchaseCatalogRow* HubRow = FindRowById(BuildingRows, HubDrop->GetPrimaryAssetId());
		Expect(HubRow != nullptr
			&& HubRow->ItemKind == EGP_PurchaseCatalogItemKind::Building
			&& HubRow->Category == EGP_PurchaseCategory::Buildings
			&& FMath::IsNearlyEqual(HubRow->Cost, HubDrop->Cost)
			&& FMath::IsNearlyEqual(HubRow->Cost, BuildingCatalog.GetPurchaseCost(HubDrop))
			&& HubRow->bEnabled,
			TEXT("D_G_HubCostFromDropDefinition"));

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Defense);
		const TArray<FGP_PurchaseCatalogRow>& DefenseRows = Presenter->GetPurchaseCatalogRows();
		Expect(HasBuildingType(DefenseRows, Tags.Building_Type_DefensiveTurret),
			TEXT("C_DefensiveTurretPresentInDefense"));
		Expect(HasWallPackage(DefenseRows), TEXT("C_WallPackagePresentInDefense"));
		Expect(!HasBuildingType(DefenseRows, Tags.Building_Type_LogisticsHub),
			TEXT("C_LogisticsHubAbsentFromDefense"));
		Expect(!HasBuildingType(DefenseRows, Tags.Building_Type_WallTurret),
			TEXT("C_WallTurretOmittedUntilSpawnedClassReady"));
		const FGP_PurchaseCatalogRow* TurretRow = FindRowById(DefenseRows, TurretDrop->GetPrimaryAssetId());
		const FGP_PurchaseCatalogRow* WallRow = FindRowById(DefenseRows, WallPackage->GetPrimaryAssetId());
		Expect(TurretRow != nullptr
			&& TurretRow->ItemKind == EGP_PurchaseCatalogItemKind::DefensiveBuilding
			&& TurretRow->Category == EGP_PurchaseCategory::Defense
			&& FMath::IsNearlyEqual(TurretRow->Cost, TurretDrop->Cost)
			&& FMath::IsNearlyEqual(TurretRow->Cost, BuildingCatalog.GetPurchaseCost(TurretDrop)),
			TEXT("D_TurretCostFromDropDefinition"));
		Expect(WallRow != nullptr
			&& WallRow->ItemKind == EGP_PurchaseCatalogItemKind::WallPackage
			&& WallRow->Category == EGP_PurchaseCategory::Defense
			&& FMath::IsNearlyEqual(WallRow->Cost, WallPackage->Cost)
			&& WallRow->SegmentCount == WallPackage->SegmentCount
			&& WallRow->bEnabled,
			TEXT("D_G_WallPackageCostAndSegmentFromDefinition"));

		{
			UGP_BuildingDefinition* AuthoredHubBuilding = NewObject<UGP_BuildingDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_Building_LogisticsHub_AuthoredNoDropTag")), RF_Transient);
			AuthoredHubBuilding->DisplayName =
				NSLOCTEXT("GPPurchaseCatalog", "AuthoredHub", "Authored Logistics Hub");
			if (Tags.Unit_Type_Building.IsValid())
			{
				AuthoredHubBuilding->BuildingTags.AddTag(Tags.Unit_Type_Building);
			}
			AuthoredHubBuilding->BuildingTags.AddTag(Tags.Building_Type_LogisticsHub);
			AuthoredHubBuilding->SpawnedClass = AGP_LogisticsHub::StaticClass();
			AuthoredHubBuilding->FootprintCells = FIntPoint(4, 4);

			UGP_OrbitalDropDefinition* AuthoredHubDrop = NewObject<UGP_OrbitalDropDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_AuthoredNoDropTag")), RF_Transient);
			AuthoredHubDrop->Cost = 77.0f;
			AuthoredHubDrop->BuildingDefinition = AuthoredHubBuilding;
			AuthoredHubDrop->DropTags.Reset();
			Expect(!Tags.Drop_Type_Building.IsValid()
				|| !AuthoredHubDrop->DropTags.HasTagExact(Tags.Drop_Type_Building),
				TEXT("AuthoredHubHasNoDropTypeBuilding"));

			BuildingCatalog.DebugAssignLoadedAuthoredLogisticsHub(AuthoredHubDrop);

			TArray<UGP_OrbitalDropDefinition*> VisibleAfterAuthored;
			BuildingCatalog.GetOperatorVisibleDrops(VisibleAfterAuthored);
			Expect(VisibleAfterAuthored.Contains(AuthoredHubDrop)
				&& BuildingCatalog.GetLegacyLogisticsHubDrop() == AuthoredHubDrop,
				TEXT("AuthoredA_OperatorVisibleIncludesAuthoredHub"));

			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
			const TArray<FGP_PurchaseCatalogRow>& AuthoredBuildingRows = Presenter->GetPurchaseCatalogRows();
			const FGP_PurchaseCatalogRow* AuthoredHubRow =
				FindRowById(AuthoredBuildingRows, AuthoredHubDrop->GetPrimaryAssetId());
			Expect(AuthoredHubRow != nullptr
				&& AuthoredBuildingRows.Num() == 1
				&& AuthoredHubRow->ItemKind == EGP_PurchaseCatalogItemKind::Building
				&& AuthoredHubRow->Category == EGP_PurchaseCategory::Buildings,
				TEXT("AuthoredB_PurchaseBuildingsShowsAuthoredHub"));
			Expect(AuthoredHubRow != nullptr
				&& AuthoredHubRow->ItemId != HubDrop->GetPrimaryAssetId()
				&& AuthoredHubRow->ItemId == AuthoredHubDrop->GetPrimaryAssetId(),
				TEXT("AuthoredC_ItemIdIsAuthoredNotNative"));
			Expect(AuthoredHubRow != nullptr
				&& FMath::IsNearlyEqual(AuthoredHubRow->Cost, AuthoredHubDrop->Cost)
				&& !FMath::IsNearlyEqual(AuthoredHubRow->Cost, HubDrop->Cost),
				TEXT("AuthoredD_CostFromAuthoredDrop"));
			Expect(AuthoredHubRow != nullptr
				&& AuthoredHubRow->DisplayName.EqualTo(AuthoredHubBuilding->DisplayName),
				TEXT("AuthoredE_DisplayNameFromLinkedBuildingDefinition"));
			Expect(AuthoredHubRow != nullptr
				&& !AuthoredHubDrop->DropTags.HasTagExact(Tags.Drop_Type_Building),
				TEXT("AuthoredF_RowDoesNotRequireDropTypeBuilding"));

			UGP_BuildingDefinition* AuthoredTurretBuilding = NewObject<UGP_BuildingDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_Building_DefensiveTurret_AuthoredGenericDrop")), RF_Transient);
			AuthoredTurretBuilding->DisplayName =
				NSLOCTEXT("GPPurchaseCatalog", "AuthoredTurret", "Authored Defensive Turret");
			if (Tags.Unit_Type_Building.IsValid())
			{
				AuthoredTurretBuilding->BuildingTags.AddTag(Tags.Unit_Type_Building);
			}
			AuthoredTurretBuilding->BuildingTags.AddTag(Tags.Building_Type_DefensiveTurret);
			AuthoredTurretBuilding->SpawnedClass = AGP_DefensiveTurret::StaticClass();
			AuthoredTurretBuilding->FootprintCells = FIntPoint(2, 2);

			UGP_OrbitalDropDefinition* AuthoredTurretDrop = NewObject<UGP_OrbitalDropDefinition>(
				GetTransientPackage(),
				FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret_AuthoredGenericDrop")),
				RF_Transient);
			AuthoredTurretDrop->Cost = 88.0f;
			AuthoredTurretDrop->BuildingDefinition = AuthoredTurretBuilding;
			AuthoredTurretDrop->DropTags.Reset();
			if (Tags.Drop_Type_Building.IsValid())
			{
				AuthoredTurretDrop->DropTags.AddTag(Tags.Drop_Type_Building);
			}
			BuildingCatalog.DebugAssignLoadedAuthoredDefensiveTurret(AuthoredTurretDrop);

			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
			const TArray<FGP_PurchaseCatalogRow>& BuildingsAfterTurret = Presenter->GetPurchaseCatalogRows();
			Expect(FindRowById(BuildingsAfterTurret, AuthoredHubDrop->GetPrimaryAssetId()) != nullptr
				&& FindRowById(BuildingsAfterTurret, AuthoredTurretDrop->GetPrimaryAssetId()) == nullptr
				&& !HasBuildingType(BuildingsAfterTurret, Tags.Building_Type_DefensiveTurret)
				&& !HasBuildingType(BuildingsAfterTurret, Tags.Building_Type_Wall)
				&& !HasBuildingType(BuildingsAfterTurret, Tags.Building_Type_MainBase)
				&& !HasBuildingType(BuildingsAfterTurret, Tags.Building_Type_WallTurret),
				TEXT("AuthoredLane_TurretWallMainBaseStayOutOfBuildings"));

			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Defense);
			const TArray<FGP_PurchaseCatalogRow>& DefenseAfterAuthored = Presenter->GetPurchaseCatalogRows();
			Expect(FindRowById(DefenseAfterAuthored, AuthoredTurretDrop->GetPrimaryAssetId()) != nullptr
				&& FindRowById(DefenseAfterAuthored, AuthoredHubDrop->GetPrimaryAssetId()) == nullptr
				&& HasBuildingType(DefenseAfterAuthored, Tags.Building_Type_DefensiveTurret)
				&& !HasBuildingType(DefenseAfterAuthored, Tags.Building_Type_LogisticsHub)
				&& !HasBuildingType(DefenseAfterAuthored, Tags.Building_Type_WallTurret),
				TEXT("AuthoredLane_TurretStaysDefenseHubAbsentWallTurretUnchanged"));

			BuildingCatalog.DebugClearAuthoredBuildingDropOverrides();
		}

		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseDefense
			&& !HasKind(Presenter->GetPurchaseCatalogRows(), EGP_PurchaseCatalogItemKind::Unit)
			&& HasKind(Presenter->GetPurchaseCatalogRows(), EGP_PurchaseCatalogItemKind::WallPackage),
			TEXT("I_CategoryNavigationChangesRowSet"));

		if (UGP_WallSegmentInventoryComponent* Inventory = MainBase->GetWallSegmentInventoryComponent())
		{
			Inventory->DebugForceSetStock(UGP_WallSegmentInventoryComponent::DefaultCapacity);
			const FGP_PurchaseCatalogRow* FullStockRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), WallPackage->GetPrimaryAssetId());
			Expect(FullStockRow != nullptr
				&& !FullStockRow->bEnabled
				&& FullStockRow->DisabledReason.ToString().Contains(TEXT("stock")),
				TEXT("WallStockFullDisablesPackage"));
			Inventory->DebugForceSetStock(0);
		}

		{
			const FSoftObjectPath UnitIconPath(
				TEXT("/Game/GrimProtocol/UI/T_GP_PurchaseIcon_UnitContractStub.T_GP_PurchaseIcon_UnitContractStub"));
			const FSoftObjectPath BuildingIconPath(
				TEXT("/Game/GrimProtocol/UI/T_GP_PurchaseIcon_BuildingContractStub.T_GP_PurchaseIcon_BuildingContractStub"));
			const FSoftObjectPath WallIconPath(
				TEXT("/Game/GrimProtocol/UI/T_GP_PurchaseIcon_WallContractStub.T_GP_PurchaseIcon_WallContractStub"));
			UTexture2D* UnitTex = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, TEXT("GPPurchaseIconUnit"));
			UTexture2D* BuildingTex = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, TEXT("GPPurchaseIconBuilding"));
			UTexture2D* WallTex = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, TEXT("GPPurchaseIconWall"));
			Expect(IsValid(UnitTex) && IsValid(BuildingTex) && IsValid(WallTex),
				TEXT("Icon_TransientTextures"));

			UGP_OrbitalUnitDropDefinition* IconWorker = NewObject<UGP_OrbitalUnitDropDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_OrbitalUnitDrop_Worker_IconStub")), RF_Transient);
			IconWorker->DisplayName = WorkerDrop->DisplayName;
			IconWorker->Cost = WorkerDrop->Cost;
			IconWorker->TransportSlotCost = WorkerDrop->TransportSlotCost;
			IconWorker->UnitDefinition = WorkerDrop->UnitDefinition;
			IconWorker->PayloadClass = WorkerDrop->PayloadClass;
			IconWorker->Icon = TSoftObjectPtr<UTexture2D>(UnitIconPath);

			UGP_OrbitalUnitDropDefinition* IconWalker = NewObject<UGP_OrbitalUnitDropDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_OrbitalUnitDrop_Walker_IconStub")), RF_Transient);
			IconWalker->DisplayName = WalkerDrop->DisplayName;
			IconWalker->Cost = WalkerDrop->Cost;
			IconWalker->TransportSlotCost = WalkerDrop->TransportSlotCost;
			IconWalker->UnitDefinition = WalkerDrop->UnitDefinition;
			IconWalker->PayloadClass = WalkerDrop->PayloadClass;
			IconWalker->Icon = TSoftObjectPtr<UTexture2D>(UnitIconPath);

			UGP_OrbitalUnitDropDefinition* EmptyIconWalker = NewObject<UGP_OrbitalUnitDropDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_OrbitalUnitDrop_Walker_EmptyIconStub")), RF_Transient);
			EmptyIconWalker->DisplayName = WalkerDrop->DisplayName;
			EmptyIconWalker->Cost = WalkerDrop->Cost;
			EmptyIconWalker->TransportSlotCost = WalkerDrop->TransportSlotCost;
			EmptyIconWalker->UnitDefinition = WalkerDrop->UnitDefinition;
			EmptyIconWalker->PayloadClass = WalkerDrop->PayloadClass;

			Presenter->DebugHoldPurchaseIconCompletion(true);
			Presenter->DebugInjectHeldPurchaseIcon(UnitIconPath, UnitTex);
			Presenter->DebugInjectHeldPurchaseIcon(BuildingIconPath, BuildingTex);
			Presenter->DebugInjectHeldPurchaseIcon(WallIconPath, WallTex);

			UnitCatalog.DebugAssignLoadedAuthoredWorker(IconWorker);
			UnitCatalog.DebugAssignLoadedAuthoredSalvageWalker(EmptyIconWalker);
			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);
			const int32 RequestsAfterEmpty = Presenter->DebugGetPurchaseIconRequestCount();
			const FGP_PurchaseCatalogRow* EmptyWalkerRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), EmptyIconWalker->GetPrimaryAssetId());
			Expect(EmptyWalkerRow != nullptr && EmptyWalkerRow->Icon == nullptr
				&& Presenter->DebugHasPendingPurchaseIcon(UnitIconPath)
				&& RequestsAfterEmpty >= 1,
				TEXT("IconEmpty_NoRequestForNullSoftAndWorkerPending"));

			UnitCatalog.DebugAssignLoadedAuthoredSalvageWalker(IconWalker);
			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);
			const int32 RequestsAfterShared = Presenter->DebugGetPurchaseIconRequestCount();
			const FGP_PurchaseCatalogRow* SharedWorkerRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWorker->GetPrimaryAssetId());
			const FGP_PurchaseCatalogRow* SharedWalkerRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWalker->GetPrimaryAssetId());
			Expect(SharedWorkerRow != nullptr && SharedWorkerRow->Icon == nullptr
				&& SharedWalkerRow != nullptr && SharedWalkerRow->Icon == nullptr
				&& RequestsAfterShared == RequestsAfterEmpty
				&& Presenter->DebugGetPendingPurchaseIconCount() == 1,
				TEXT("IconDedup_SharedPathRequestsOnceAndRowsNullUntilReady"));

			Presenter->DebugCompleteHeldPurchaseIconLoad(UnitIconPath);
			const FGP_PurchaseCatalogRow* ReadyWorkerRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWorker->GetPrimaryAssetId());
			const FGP_PurchaseCatalogRow* ReadyWalkerRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWalker->GetPrimaryAssetId());
			Expect(ReadyWorkerRow != nullptr && ReadyWorkerRow->Icon == UnitTex
				&& ReadyWalkerRow != nullptr && ReadyWalkerRow->Icon == UnitTex,
				TEXT("IconA_UnitDropIconValidAfterAsyncComplete"));

			UGP_BuildingDefinition* IconHubBuilding = NewObject<UGP_BuildingDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_Building_LogisticsHub_IconStub")), RF_Transient);
			IconHubBuilding->DisplayName = NSLOCTEXT("GPPurchaseCatalog", "IconHub", "Icon Logistics Hub");
			if (Tags.Unit_Type_Building.IsValid())
			{
				IconHubBuilding->BuildingTags.AddTag(Tags.Unit_Type_Building);
			}
			IconHubBuilding->BuildingTags.AddTag(Tags.Building_Type_LogisticsHub);
			IconHubBuilding->SpawnedClass = AGP_LogisticsHub::StaticClass();
			IconHubBuilding->FootprintCells = FIntPoint(4, 4);
			IconHubBuilding->Icon = TSoftObjectPtr<UTexture2D>(BuildingIconPath);

			UGP_OrbitalDropDefinition* IconHubDrop = NewObject<UGP_OrbitalDropDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_IconStub")), RF_Transient);
			IconHubDrop->Cost = HubDrop->Cost;
			IconHubDrop->BuildingDefinition = IconHubBuilding;

			BuildingCatalog.DebugAssignLoadedAuthoredLogisticsHub(IconHubDrop);
			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
			const FGP_PurchaseCatalogRow* HeldHubRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconHubDrop->GetPrimaryAssetId());
			Expect(HeldHubRow != nullptr && HeldHubRow->Icon == nullptr
				&& Presenter->DebugHasPendingPurchaseIcon(BuildingIconPath),
				TEXT("IconB_BuildingIconNullUntilReady"));
			Presenter->RequestPurchaseRowPrimary(IconHubDrop->GetPrimaryAssetId());
			Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseBuildingSelected
				&& Presenter->GetSelectedPurchaseItem().Icon == nullptr,
				TEXT("IconB_SelectedNullWhileHeld"));
			Presenter->DebugCompleteHeldPurchaseIconLoad(BuildingIconPath);
			Expect(Presenter->GetSelectedPurchaseItem().ItemId == IconHubDrop->GetPrimaryAssetId()
				&& Presenter->GetSelectedPurchaseItem().Icon == BuildingTex,
				TEXT("IconB_SelectedBuildingIconValidAfterAsyncComplete"));
			Presenter->RequestPurchaseBack();
			const FGP_PurchaseCatalogRow* ReadyHubRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconHubDrop->GetPrimaryAssetId());
			Expect(ReadyHubRow != nullptr && ReadyHubRow->Icon == BuildingTex,
				TEXT("IconB_BuildingListIconValidAfterAsyncComplete"));

			UGP_WallPackageDefinition* IconWall = NewObject<UGP_WallPackageDefinition>(
				GetTransientPackage(), FName(TEXT("DA_GP_WallPackage_IconStub")), RF_Transient);
			IconWall->DisplayName = WallPackage->DisplayName;
			IconWall->Cost = WallPackage->Cost;
			IconWall->SegmentCount = WallPackage->SegmentCount;
			IconWall->Icon = TSoftObjectPtr<UTexture2D>(WallIconPath);
			if (WallCatalog != nullptr)
			{
				WallCatalog->DebugAssignLoadedAuthored(IconWall);
			}
			Presenter->RequestPurchaseBack();
			Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Defense);
			const FGP_PurchaseCatalogRow* HeldWallRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWall->GetPrimaryAssetId());
			Expect(HeldWallRow != nullptr && HeldWallRow->Icon == nullptr
				&& Presenter->DebugHasPendingPurchaseIcon(WallIconPath),
				TEXT("IconC_WallPackageIconNullUntilReady"));
			Presenter->RequestPurchaseRowPrimary(IconWall->GetPrimaryAssetId());
			Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseDefenseSelected
				&& Presenter->GetSelectedPurchaseItem().Icon == nullptr,
				TEXT("IconC_SelectedWallNullWhileHeld"));
			Presenter->DebugCompleteHeldPurchaseIconLoad(WallIconPath);
			Expect(Presenter->GetSelectedPurchaseItem().Icon == WallTex,
				TEXT("IconC_SelectedWallIconValidAfterAsyncComplete"));
			Presenter->RequestPurchaseBack();
			const FGP_PurchaseCatalogRow* ReadyWallRow =
				FindRowById(Presenter->GetPurchaseCatalogRows(), IconWall->GetPrimaryAssetId());
			Expect(ReadyWallRow != nullptr && ReadyWallRow->Icon == WallTex,
				TEXT("IconC_WallPackageIconValidAfterAsyncComplete"));

			const int32 RequestsBeforeCancel = Presenter->DebugGetPurchaseIconRequestCount();
			Presenter->DebugCancelPurchaseIconLoads();
			Presenter->DebugCompleteHeldPurchaseIconLoad(WallIconPath);
			Expect(Presenter->DebugGetPendingPurchaseIconCount() == 0
				&& Presenter->DebugGetPurchaseIconRequestCount() == RequestsBeforeCancel,
				TEXT("IconShutdown_HeldCompletionAfterCancelIsNoOp"));

			Presenter->DebugHoldPurchaseIconCompletion(false);
			UnitCatalog.DebugClearAuthoredUnitDropOverrides();
			BuildingCatalog.DebugClearAuthoredBuildingDropOverrides();
			if (WallCatalog != nullptr)
			{
				WallCatalog->DebugClearAuthoredOverrides();
			}
		}

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);

		UGP_OrbitalUnitDropDefinition* InjectedWorkerDrop = NewObject<UGP_OrbitalUnitDropDefinition>(
			GetTransientPackage(), FName(TEXT("DA_GP_OrbitalUnitDrop_Worker_PendingStub")), RF_Transient);
		InjectedWorkerDrop->Cost = WorkerDrop->Cost;
		InjectedWorkerDrop->TransportSlotCost = WorkerDrop->TransportSlotCost;
		UnitCatalog.DebugForceUnresolvedAuthoredWorkerLoad(InjectedWorkerDrop, true);
		Expect(UnitCatalog.DebugDidRequestAsyncAuthoredWorkerLoad()
			&& UnitCatalog.IsWorkerDropDefinitionPending()
			&& UnitCatalog.GetWorkerDrop() == nullptr,
			TEXT("J_PendingWorkerOmitsCanonicalWithoutSyncLoad"));

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Units);
		Expect(FindRowById(Presenter->GetPurchaseCatalogRows(), WorkerDrop->GetPrimaryAssetId()) == nullptr
			&& FindRowById(Presenter->GetPurchaseCatalogRows(), WalkerDrop->GetPrimaryAssetId()) != nullptr,
			TEXT("J_PendingWorkerRowOmittedWalkerRemains"));
		UnitCatalog.DebugClearAuthoredUnitDropOverrides();

		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
		UGP_OrbitalDropDefinition* InjectedHubDrop = NewObject<UGP_OrbitalDropDefinition>(
			GetTransientPackage(), FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_PendingStub")), RF_Transient);
		InjectedHubDrop->Cost = HubDrop->Cost;
		InjectedHubDrop->BuildingDefinition = HubDrop->BuildingDefinition;
		BuildingCatalog.DebugForceUnresolvedAuthoredLogisticsHubLoad(InjectedHubDrop, true);
		Presenter->RequestPurchaseBack();
		Presenter->RequestOpenPurchaseCategory(EGP_PurchaseCategory::Buildings);
		const FGP_PurchaseCatalogRow* PendingHubRow =
			FindRowById(Presenter->GetPurchaseCatalogRows(), HubDrop->GetPrimaryAssetId());
		Expect(BuildingCatalog.IsDropDefinitionPending(HubDrop)
			&& PendingHubRow != nullptr
			&& !PendingHubRow->bEnabled
			&& PendingHubRow->DisabledReason.ToString().Contains(TEXT("DefinitionNotReady")),
			TEXT("J_PendingHubKeepsNativeIdentityDisabled"));
		BuildingCatalog.DebugClearAuthoredBuildingDropOverrides();

		DestroySpawned();
		EndIsolation();
		UE_LOG(LogGPPurchaseCatalogPresentationContract, Log,
			TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GPurchaseCatalogPresentationContract(
		TEXT("gp.UI.RunPurchaseCatalogPresentationContractTest"),
		TEXT("Run production HUD Purchase catalog presentation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunPurchaseCatalogPresentationContractTest));
}

#endif
