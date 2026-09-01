// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPContextActionPresenter.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopeExit.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPurchaseCatalog, Log, All);

#define LOCTEXT_NAMESPACE "GPContextActions"

namespace GPPurchaseCatalogPresentationPrivate
{
	enum class EBuildingDropLane : uint8
	{
		Skip,
		OrdinaryBuilding,
		DefensiveTurret,
		WallTurret
	};

	static bool HasExact(const FGameplayTagContainer& Container, const FGameplayTag& Tag)
	{
		return Tag.IsValid() && Container.HasTagExact(Tag);
	}

	static UTexture2D* PeekLoadedTexture(const TSoftObjectPtr<UTexture2D>& Soft)
	{
		if (Soft.IsNull())
		{
			return nullptr;
		}
		if (UTexture2D* Loaded = Soft.Get())
		{
			return Loaded;
		}
		return Cast<UTexture2D>(Soft.ToSoftObjectPath().ResolveObject());
	}

	static EBuildingDropLane ClassifyBuildingDrop(const UGP_OrbitalDropDefinition* Drop)
	{
		if (!IsValid(Drop))
		{
			return EBuildingDropLane::Skip;
		}

		const FGPGameplayTags& Tags = FGPGameplayTags::Get();
		const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition();

		if (Building != nullptr && HasExact(Building->BuildingTags, Tags.Building_Type_MainBase))
		{
			return EBuildingDropLane::Skip;
		}
		if (HasExact(Drop->DropTags, Tags.Drop_Type_WallPackage))
		{
			return EBuildingDropLane::Skip;
		}
		if (HasExact(Drop->DropTags, Tags.Drop_Type_Wall)
			|| (Building != nullptr && HasExact(Building->BuildingTags, Tags.Building_Type_Wall)))
		{
			return EBuildingDropLane::Skip;
		}
		if (Building != nullptr && HasExact(Building->BuildingTags, Tags.Building_Type_DefensiveTurret))
		{
			return EBuildingDropLane::DefensiveTurret;
		}
		if (Building != nullptr && HasExact(Building->BuildingTags, Tags.Building_Type_WallTurret))
		{
			// Operator-visible Wall Turret only when spawned class is already resolved.
			// Native bootstrap has no SpawnedClass — omit; do not invent a row.
			return Building->ResolveLoadedSpawnedClass() != nullptr
				? EBuildingDropLane::WallTurret
				: EBuildingDropLane::Skip;
		}
		if (Building != nullptr && HasExact(Building->BuildingTags, Tags.Building_Type_LogisticsHub))
		{
			return EBuildingDropLane::OrdinaryBuilding;
		}
		if (HasExact(Drop->DropTags, Tags.Drop_Type_Building))
		{
			return EBuildingDropLane::OrdinaryBuilding;
		}

		return EBuildingDropLane::Skip;
	}

#if !UE_BUILD_SHIPPING
	static void LogSkippedOperatorVisibleDrop(const UGP_OrbitalDropDefinition* Drop)
	{
		if (!IsValid(Drop))
		{
			return;
		}

		const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition();
		UE_LOG(LogGPPurchaseCatalog, Log,
			TEXT("GP PurchaseCatalog SkipBuilding Drop=%s DropTags=%s BuildingTags=%s"),
			*Drop->GetName(),
			*Drop->DropTags.ToStringSimple(),
			Building != nullptr ? *Building->BuildingTags.ToStringSimple() : TEXT("<none>"));
	}
#endif

	static void ApplyAvailabilityGates(
		FGP_PurchaseCatalogRow& Row,
		float OrbitalFerronite,
		bool bProductReady,
		const FText& ExtraDisabledReason)
	{
		Row.bVisible = true;
		Row.bEnabled = true;
		Row.DisabledReason = FText::GetEmpty();

		auto Disable = [&Row](const FText& Reason)
		{
			Row.bEnabled = false;
			Row.DisabledReason = Reason;
		};

		if (!bProductReady)
		{
			Disable(LOCTEXT("PurchaseDefinitionNotReady", "DefinitionNotReady"));
			return;
		}
		if (!FMath::IsFinite(Row.Cost) || Row.Cost < 0.0f)
		{
			Disable(LOCTEXT("PurchaseInvalidCost", "InvalidCost"));
			return;
		}
		if (!ExtraDisabledReason.IsEmpty())
		{
			Disable(ExtraDisabledReason);
			return;
		}
		if (OrbitalFerronite + KINDA_SMALL_NUMBER < Row.Cost)
		{
			Disable(LOCTEXT("PurchaseInsufficientOrbital", "Insufficient Orbital Ferronite"));
		}
	}

	static FGP_PurchaseCatalogRow MakeBuildingRow(
		UGP_OrbitalDropDefinition* Drop,
		EGP_PurchaseCatalogItemKind ItemKind,
		EGP_PurchaseCategory Category,
		float OrbitalFerronite,
		bool bProductReady,
		UTexture2D* Icon)
	{
		FGP_PurchaseCatalogRow Row;
		Row.ItemId = Drop->GetPrimaryAssetId();
		Row.ItemKind = ItemKind;
		Row.Category = Category;
		Row.DisplayName = Drop->GetAcquisitionDisplayName();
		Row.Icon = Icon;
		Row.Cost = Drop->Cost;
		ApplyAvailabilityGates(Row, OrbitalFerronite, bProductReady, FText::GetEmpty());
		return Row;
	}
}

bool UGP_ContextActionPresenter::Initialize(AGP_PlayerController* InPlayerController)
{
	Shutdown();
	if (!IsValid(InPlayerController) || !InPlayerController->IsLocalController())
	{
		RebuildPresentation();
		return false;
	}

	UGP_SelectionComponent* Selection = InPlayerController->GetSelectionComponent();
	if (!IsValid(Selection))
	{
		RebuildPresentation();
		return false;
	}

	BoundPlayerController = InPlayerController;
	BoundSelection = Selection;
	bPurchaseIconLoadsAbandoned = false;
	BindUnitDropCatalog();
	SelectionChangedHandle = Selection->OnSelectionChanged().AddUObject(
		this, &ThisClass::HandleSelectionChanged);
	CommandTargetingChangedHandle = InPlayerController->OnCommandTargetingModeChanged().AddUObject(
		this, &ThisClass::HandleCommandTargetingModeChanged);
	RebuildPresentation();
	return true;
}

void UGP_ContextActionPresenter::Shutdown()
{
	bPurchaseIconLoadsAbandoned = true;
	CancelPurchaseIconLoads();
	UnbindUnitDropCatalog();
	UnbindWallInventory();
	UnbindBuildingInventory();
	UnbindOrbitalFerronite();
	UnbindSelectedUnits();
	if (AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (CommandTargetingChangedHandle.IsValid())
		{
			PlayerController->OnCommandTargetingModeChanged().Remove(CommandTargetingChangedHandle);
		}
	}
	CommandTargetingChangedHandle.Reset();
	if (UGP_SelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged().Remove(SelectionChangedHandle);
	}
	SelectionChangedHandle.Reset();
	BoundSelection.Reset();
	BoundPlayerController.Reset();
	RebuildPresentation();
}

void UGP_ContextActionPresenter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

int32 UGP_ContextActionPresenter::GetBoundDelegateCount() const
{
	int32 Count = SelectionChangedHandle.IsValid() ? 1 : 0;
	Count += CommandTargetingChangedHandle.IsValid() ? 1 : 0;
	Count += UnitDropCatalogChangedHandle.IsValid() ? 1 : 0;
	Count += OrbitalFerroniteHandle.IsValid() ? 1 : 0;
	Count += BuildingReadyChangedHandle.IsValid() ? 1 : 0;
	Count += BoundWallInventory.IsValid() ? 2 : 0;
	for (const FBoundSelectedUnit& Bound : BoundUnits)
	{
		Count += Bound.DiedHandle.IsValid() ? 1 : 0;
		Count += Bound.Unit.IsValid() ? 1 : 0;
	}
	return Count;
}

void UGP_ContextActionPresenter::BindUnitDropCatalog()
{
	UnbindUnitDropCatalog();
	UGP_OrbitalUnitDropCatalog& Catalog = UGP_OrbitalUnitDropCatalog::Get();
	UnitDropCatalogChangedHandle = Catalog.OnCatalogChanged.AddUObject(
		this, &ThisClass::HandleUnitDropCatalogChanged);
}

void UGP_ContextActionPresenter::UnbindUnitDropCatalog()
{
	if (!UnitDropCatalogChangedHandle.IsValid())
	{
		return;
	}
	if (UGP_OrbitalUnitDropCatalog* Catalog = UGP_OrbitalUnitDropCatalog::TryGetExisting())
	{
		Catalog->OnCatalogChanged.Remove(UnitDropCatalogChangedHandle);
	}
	UnitDropCatalogChangedHandle.Reset();
}

void UGP_ContextActionPresenter::HandleUnitDropCatalogChanged()
{
	if (!IsValid(this) || !BoundPlayerController.IsValid())
	{
		return;
	}
	if (PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		RefreshPurchaseCatalogIfCategoryActive();
	}
}

void UGP_ContextActionPresenter::HandleSelectionChanged()
{
	RebuildPresentation();
}

void UGP_ContextActionPresenter::HandleCommandTargetingModeChanged()
{
	RebuildPresentation();
}

void UGP_ContextActionPresenter::HandleUnitDied(AGP_UnitBase* Unit)
{
	(void)Unit;
	RebuildPresentation();
}

void UGP_ContextActionPresenter::HandleBoundActorDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	RebuildPresentation();
}

void UGP_ContextActionPresenter::CollectLiveSelectedUnits(
	const UGP_SelectionComponent* Selection,
	TArray<AGP_UnitBase*>& OutUnits)
{
	OutUnits.Reset();
	if (Selection == nullptr)
	{
		return;
	}

	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : Selection->GetSelectedUnits())
	{
		AGP_UnitBase* Unit = WeakUnit.Get();
		if (IsValid(Unit) && !Unit->IsDead() && !Unit->IsActorBeingDestroyed())
		{
			OutUnits.Add(Unit);
		}
	}
}

EGP_ContextActionMode UGP_ContextActionPresenter::ResolveMode(
	const TArray<AGP_UnitBase*>& LiveSelected,
	int32 LocalTeamId)
{
	if (LiveSelected.Num() == 0)
	{
		return EGP_ContextActionMode::None;
	}

	if (LiveSelected.Num() == 1)
	{
		AGP_UnitBase* Unit = LiveSelected[0];
		if (Cast<AGP_MainBase>(Unit) != nullptr
			&& LocalTeamId >= 1
			&& Unit->GetTeamId() == LocalTeamId)
		{
			return EGP_ContextActionMode::MainBase;
		}
		if (Cast<AGP_MainBase>(Unit) != nullptr)
		{
			return EGP_ContextActionMode::None;
		}
		if (Unit->IsSelectionTypeBuilding())
		{
			return EGP_ContextActionMode::Building;
		}
		if (Unit->IsSelectionTypeUnit())
		{
			return EGP_ContextActionMode::Unit;
		}
		return EGP_ContextActionMode::None;
	}

	bool bAllUnits = true;
	for (const AGP_UnitBase* Unit : LiveSelected)
	{
		if (Unit == nullptr || !Unit->IsSelectionTypeUnit() || Unit->IsSelectionTypeBuilding())
		{
			bAllUnits = false;
			break;
		}
	}

	return bAllUnits ? EGP_ContextActionMode::UnitGroup : EGP_ContextActionMode::None;
}

FGP_ContextActionPresentation UGP_ContextActionPresenter::MakeAction(
	EGP_ContextActionId ActionId,
	const FText& DisplayName,
	bool bEnabled,
	const FText& DisabledReason)
{
	FGP_ContextActionPresentation Row;
	Row.ActionId = ActionId;
	Row.DisplayName = DisplayName;
	Row.bVisible = true;
	Row.bEnabled = bEnabled;
	Row.DisabledReason = bEnabled ? FText::GetEmpty() : DisabledReason;
	return Row;
}

void UGP_ContextActionPresenter::UnbindSelectedUnits()
{
	for (FBoundSelectedUnit& Bound : BoundUnits)
	{
		if (AGP_UnitBase* Unit = Bound.Unit.Get())
		{
			if (Bound.DiedHandle.IsValid())
			{
				Unit->OnUnitDied().Remove(Bound.DiedHandle);
			}
			Unit->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleBoundActorDestroyed);
		}
		Bound.DiedHandle.Reset();
	}
	BoundUnits.Reset();
}

void UGP_ContextActionPresenter::BindSelectedUnits(const TArray<AGP_UnitBase*>& Units)
{
	BoundUnits.Reserve(Units.Num());
	for (AGP_UnitBase* Unit : Units)
	{
		if (!IsValid(Unit))
		{
			continue;
		}

		FBoundSelectedUnit Bound;
		Bound.Unit = Unit;
		Unit->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleBoundActorDestroyed);
		Bound.DiedHandle = Unit->OnUnitDied().AddUObject(this, &ThisClass::HandleUnitDied);
		BoundUnits.Add(MoveTemp(Bound));
	}
}

void UGP_ContextActionPresenter::EnsureOrbitalFerroniteBinding()
{
	UAbilitySystemComponent* ASC = nullptr;
	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			ASC = PlayerState->GetAbilitySystemComponent();
		}
	}

	if (BoundOrbitalASC.Get() == ASC && OrbitalFerroniteHandle.IsValid())
	{
		return;
	}

	UnbindOrbitalFerronite();
	if (!IsValid(ASC))
	{
		return;
	}

	BoundOrbitalASC = ASC;
	OrbitalFerroniteHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).AddUObject(
			this, &ThisClass::HandleOrbitalFerroniteChanged);
}

void UGP_ContextActionPresenter::UnbindOrbitalFerronite()
{
	if (UAbilitySystemComponent* ASC = BoundOrbitalASC.Get())
	{
		if (OrbitalFerroniteHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).Remove(OrbitalFerroniteHandle);
		}
	}
	OrbitalFerroniteHandle.Reset();
	BoundOrbitalASC.Reset();
}

void UGP_ContextActionPresenter::BindWallInventory(AGP_MainBase* MainBase)
{
	UnbindWallInventory();
	if (!IsValid(MainBase))
	{
		return;
	}

	UGP_WallSegmentInventoryComponent* Inventory = MainBase->GetWallSegmentInventoryComponent();
	if (!IsValid(Inventory))
	{
		return;
	}

	BoundWallInventory = Inventory;
	Inventory->OnWallInventoryChanged.AddDynamic(this, &ThisClass::HandleWallInventoryChanged);
	Inventory->OnWallPackagePendingChanged.AddDynamic(this, &ThisClass::HandleWallPackagePendingChanged);
}

void UGP_ContextActionPresenter::UnbindWallInventory()
{
	if (UGP_WallSegmentInventoryComponent* Inventory = BoundWallInventory.Get())
	{
		Inventory->OnWallInventoryChanged.RemoveDynamic(this, &ThisClass::HandleWallInventoryChanged);
		Inventory->OnWallPackagePendingChanged.RemoveDynamic(this, &ThisClass::HandleWallPackagePendingChanged);
	}
	BoundWallInventory.Reset();
}

void UGP_ContextActionPresenter::HandleOrbitalFerroniteChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;
	RefreshPurchaseCatalogIfCategoryActive();
}

void UGP_ContextActionPresenter::HandleWallInventoryChanged(int32 NewCount)
{
	(void)NewCount;
	RefreshPurchaseCatalogIfCategoryActive();
}

void UGP_ContextActionPresenter::HandleWallPackagePendingChanged(bool bPending)
{
	(void)bPending;
	RefreshPurchaseCatalogIfCategoryActive();
}

void UGP_ContextActionPresenter::RefreshPurchaseCatalogIfCategoryActive()
{
	if (!IsPurchasePresentationState(PanelState))
	{
		return;
	}

	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
}

void UGP_ContextActionPresenter::RebuildPurchaseCatalogRows()
{
	using namespace GPPurchaseCatalogPresentationPrivate;

	bInsidePurchaseCatalogRebuild = true;
	ON_SCOPE_EXIT
	{
		bInsidePurchaseCatalogRebuild = false;
		if (bPurchaseIconRefreshDeferred)
		{
			bPurchaseIconRefreshDeferred = false;
			RefreshPurchaseCatalogIfCategoryActive();
		}
	};

	PurchaseCatalogRows.Reset();
	if (Mode != EGP_ContextActionMode::MainBase)
	{
		SelectedPurchaseRow = FGP_PurchaseCatalogRow();
		return;
	}

	if (IsPurchaseSelectedState(PanelState))
	{
		RebuildSelectedPurchaseRow();
		return;
	}

	if (!IsPurchaseCategoryState(PanelState))
	{
		SelectedPurchaseRow = FGP_PurchaseCatalogRow();
		return;
	}

	float OrbitalFerronite = 0.0f;
	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			if (const UGP_PlayerAttributeSet* Attributes = PlayerState->GetPlayerAttributeSet())
			{
				OrbitalFerronite = Attributes->GetOrbitalFerronite();
			}
		}
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		UGP_OrbitalUnitDropCatalog& UnitCatalog = UGP_OrbitalUnitDropCatalog::Get();
		auto AddUnitRow = [&](UGP_OrbitalUnitDropDefinition* Drop)
		{
			if (!IsValid(Drop))
			{
				return;
			}

			FGP_PurchaseCatalogRow Row;
			Row.ItemId = Drop->GetPrimaryAssetId();
			Row.ItemKind = EGP_PurchaseCatalogItemKind::Unit;
			Row.Category = EGP_PurchaseCategory::Units;
			Row.DisplayName = Drop->DisplayName;
			Row.Icon = ResolveUnitPurchaseIcon(Drop);
			Row.Cost = Drop->Cost;
			Row.TransportSlotCost = Drop->TransportSlotCost;
			if (const UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitCatalog.GetWorkerDrop())
			{
				if (WorkerDrop->GetPrimaryAssetId() == Drop->GetPrimaryAssetId())
				{
					Row.Quantity = PendingUnitManifest.WorkerCount;
				}
			}
			if (const UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitCatalog.GetSalvageWalkerDrop())
			{
				if (WalkerDrop->GetPrimaryAssetId() == Drop->GetPrimaryAssetId())
				{
					Row.Quantity = PendingUnitManifest.SalvageWalkerCount;
				}
			}
			ApplyAvailabilityGates(Row, OrbitalFerronite, true, FText::GetEmpty());
			PurchaseCatalogRows.Add(MoveTemp(Row));
		};

		// Canonical ready only. Pending authored → GetWorkerDrop()/GetSalvageWalkerDrop() are null; omit.
		AddUnitRow(UnitCatalog.GetWorkerDrop());
		AddUnitRow(UnitCatalog.GetSalvageWalkerDrop());
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseBuildings)
	{
		UGP_BuildingDropCatalog& BuildingCatalog = UGP_BuildingDropCatalog::Get();
		TArray<UGP_OrbitalDropDefinition*> VisibleDrops;
		BuildingCatalog.GetOperatorVisibleDrops(VisibleDrops);
		for (UGP_OrbitalDropDefinition* Drop : VisibleDrops)
		{
			const EBuildingDropLane Lane = ClassifyBuildingDrop(Drop);
			if (Lane != EBuildingDropLane::OrdinaryBuilding)
			{
#if !UE_BUILD_SHIPPING
				if (Lane == EBuildingDropLane::Skip)
				{
					LogSkippedOperatorVisibleDrop(Drop);
				}
#endif
				continue;
			}

			const bool bReady = !BuildingCatalog.IsDropDefinitionPending(Drop)
				&& Drop->ResolveLoadedBuildingDefinition() != nullptr;
			UTexture2D* Icon = nullptr;
			if (const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition())
			{
				Icon = ResolveBuildingPurchaseIcon(Building);
			}
			PurchaseCatalogRows.Add(MakeBuildingRow(
				Drop,
				EGP_PurchaseCatalogItemKind::Building,
				EGP_PurchaseCategory::Buildings,
				OrbitalFerronite,
				bReady,
				Icon));
		}
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseDefense)
	{
		UGP_BuildingDropCatalog& BuildingCatalog = UGP_BuildingDropCatalog::Get();
		TArray<UGP_OrbitalDropDefinition*> VisibleDrops;
		BuildingCatalog.GetOperatorVisibleDrops(VisibleDrops);
		for (UGP_OrbitalDropDefinition* Drop : VisibleDrops)
		{
			const EBuildingDropLane Lane = ClassifyBuildingDrop(Drop);
			if (Lane != EBuildingDropLane::DefensiveTurret && Lane != EBuildingDropLane::WallTurret)
			{
#if !UE_BUILD_SHIPPING
				if (Lane == EBuildingDropLane::Skip)
				{
					LogSkippedOperatorVisibleDrop(Drop);
				}
#endif
				continue;
			}

			const bool bReady = !BuildingCatalog.IsDropDefinitionPending(Drop)
				&& Drop->ResolveLoadedBuildingDefinition() != nullptr;
			UTexture2D* Icon = nullptr;
			if (const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition())
			{
				Icon = ResolveBuildingPurchaseIcon(Building);
			}
			PurchaseCatalogRows.Add(MakeBuildingRow(
				Drop,
				EGP_PurchaseCatalogItemKind::DefensiveBuilding,
				EGP_PurchaseCategory::Defense,
				OrbitalFerronite,
				bReady,
				Icon));
		}

		if (UGP_WallPackageCatalog* WallCatalog = UGP_WallPackageCatalog::Get())
		{
			if (UGP_WallPackageDefinition* Package = WallCatalog->GetWallPackage())
			{
				FGP_PurchaseCatalogRow Row;
				Row.ItemId = Package->GetPrimaryAssetId();
				Row.ItemKind = EGP_PurchaseCatalogItemKind::WallPackage;
				Row.Category = EGP_PurchaseCategory::Defense;
				Row.DisplayName = Package->DisplayName;
				Row.Icon = ResolvePurchaseIcon(Package->Icon);
				Row.Cost = Package->Cost;
				Row.SegmentCount = Package->SegmentCount;

				FText ExtraReason;
				if (const UGP_WallSegmentInventoryComponent* Inventory = BoundWallInventory.Get())
				{
					if (Inventory->IsWallPackagePending())
					{
						ExtraReason = LOCTEXT("PurchaseWallDeliveryPending", "Delivery already pending");
					}
					else if (!Inventory->CanPurchaseWallPackage())
					{
						ExtraReason = LOCTEXT("PurchaseWallStockFull", "Wall stock full");
					}
				}

				ApplyAvailabilityGates(Row, OrbitalFerronite, true, ExtraReason);
				PurchaseCatalogRows.Add(MoveTemp(Row));
			}
		}
	}
}

void UGP_ContextActionPresenter::RebuildPresentation()
{
	UnbindWallInventory();
	UnbindSelectedUnits();

	TArray<AGP_UnitBase*> LiveSelected;
	CollectLiveSelectedUnits(BoundSelection.Get(), LiveSelected);
	BindSelectedUnits(LiveSelected);
	EnsureOrbitalFerroniteBinding();
	EnsureBuildingInventoryBinding();

	int32 LocalTeamId = -1;
	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			LocalTeamId = PlayerState->GetTeamId();
		}
	}

	Mode = ResolveMode(LiveSelected, LocalTeamId);
	if (Mode != EGP_ContextActionMode::MainBase)
	{
		PanelState = EGP_ContextActionPanelState::Actions;
		ClearPurchaseSession();
	}

	Actions.Reset();
	switch (Mode)
	{
	case EGP_ContextActionMode::Unit:
	case EGP_ContextActionMode::UnitGroup:
	{
		const AGP_PlayerController* PlayerController = BoundPlayerController.Get();
		const bool bMoveEnabled =
			PlayerController != nullptr && PlayerController->SelectionHasMoveEligibleUnit();
		const bool bAttackMoveEnabled =
			PlayerController != nullptr && PlayerController->SelectionHasAttackMoveEligibleUnit();
		const bool bPatrolEnabled =
			PlayerController != nullptr && PlayerController->SelectionHasPatrolEligibleUnit();
		Actions.Add(MakeAction(
			EGP_ContextActionId::Move,
			LOCTEXT("Move", "Move"),
			bMoveEnabled,
			bMoveEnabled
				? FText::GetEmpty()
				: LOCTEXT("MoveIneligible", "Selection has no Move eligible units")));
		Actions.Add(MakeAction(
			EGP_ContextActionId::Stop,
			LOCTEXT("Stop", "Stop"),
			true,
			FText::GetEmpty()));
		Actions.Add(MakeAction(
			EGP_ContextActionId::AttackMove,
			LOCTEXT("AttackMove", "Attack-Move"),
			bAttackMoveEnabled,
			bAttackMoveEnabled
				? FText::GetEmpty()
				: LOCTEXT("AttackMoveIneligible", "Selection has no Attack-Move eligible units")));
		Actions.Add(MakeAction(
			EGP_ContextActionId::Patrol,
			LOCTEXT("Patrol", "Patrol"),
			bPatrolEnabled,
			bPatrolEnabled
				? FText::GetEmpty()
				: LOCTEXT("PatrolIneligible", "Selection has no Patrol eligible units")));
		break;
	}
	case EGP_ContextActionMode::MainBase:
		Actions.Add(MakeAction(
			EGP_ContextActionId::Purchase,
			LOCTEXT("Purchase", "Purchase"),
			true,
			FText::GetEmpty()));
		break;
	case EGP_ContextActionMode::Building:
	case EGP_ContextActionMode::None:
	default:
		break;
	}

	AGP_MainBase* SelectedMainBase = nullptr;
	if (Mode == EGP_ContextActionMode::MainBase && LiveSelected.Num() == 1)
	{
		SelectedMainBase = Cast<AGP_MainBase>(LiveSelected[0]);
	}
	BindWallInventory(SelectedMainBase);

	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
}

FText UGP_ContextActionPresenter::GetTargetingPrompt() const
{
	const AGP_PlayerController* PlayerController = BoundPlayerController.Get();
	if (PlayerController == nullptr)
	{
		return FText::GetEmpty();
	}

	switch (PlayerController->GetCommandTargetingMode())
	{
	case EGP_CommandTargetingMode::Move:
		return LOCTEXT("PromptMove", "MOVE: Select destination");
	case EGP_CommandTargetingMode::AttackMove:
		return LOCTEXT("PromptAttackMove", "ATTACK MOVE: Select destination");
	case EGP_CommandTargetingMode::Patrol:
		return LOCTEXT("PromptPatrol", "PATROL: Select patrol point");
	default:
		return FText::GetEmpty();
	}
}

FText UGP_ContextActionPresenter::GetContextMessage() const
{
	const FText Targeting = GetTargetingPrompt();
	if (!Targeting.IsEmpty())
	{
		return Targeting;
	}

	if (!PurchaseContextMessage.IsEmpty()
		&& PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		return PurchaseContextMessage;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		const FGP_PurchaseUnitManifestPresentation Manifest = BuildUnitManifestPresentation();
		if (Manifest.UnitCount <= 0)
		{
			return FText::Format(
				LOCTEXT("ShuttleEmpty", "Shuttle capacity: {0} / {1} slots"),
				FText::AsNumber(0),
				FText::AsNumber(Manifest.MaxSlots));
		}

		return FText::Format(
			LOCTEXT("ShuttleUsed", "Shuttle: {0} / {1} slots"),
			FText::AsNumber(Manifest.UsedSlots),
			FText::AsNumber(Manifest.MaxSlots));
	}

	return FText::GetEmpty();
}

FGP_PurchaseCatalogRow UGP_ContextActionPresenter::GetSelectedPurchaseItem() const
{
	if (!IsPurchaseSelectedState(PanelState))
	{
		return FGP_PurchaseCatalogRow();
	}
	return SelectedPurchaseRow;
}

FGP_PurchaseUnitManifestPresentation UGP_ContextActionPresenter::GetPurchaseUnitManifestPresentation() const
{
	return BuildUnitManifestPresentation();
}

bool UGP_ContextActionPresenter::IsActionEnabled(EGP_ContextActionId ActionId) const
{
	for (const FGP_ContextActionPresentation& Row : Actions)
	{
		if (Row.ActionId == ActionId)
		{
			return Row.bVisible && Row.bEnabled;
		}
	}
	return false;
}

void UGP_ContextActionPresenter::RequestContextAction(EGP_ContextActionId ActionId)
{
	if (!IsActionEnabled(ActionId))
	{
		return;
	}

	AGP_PlayerController* PlayerController = BoundPlayerController.Get();
	switch (ActionId)
	{
	case EGP_ContextActionId::AttackMove:
		if (PlayerController != nullptr)
		{
			PlayerController->EnterAttackMoveMode();
		}
		break;
	case EGP_ContextActionId::Stop:
		if (PlayerController != nullptr)
		{
			PlayerController->RequestStopSelectedUnits();
		}
		break;
	case EGP_ContextActionId::Purchase:
		RequestOpenMainBasePurchase();
		break;
	case EGP_ContextActionId::Move:
		if (PlayerController != nullptr)
		{
			PlayerController->EnterMoveMode();
		}
		break;
	case EGP_ContextActionId::Patrol:
		if (PlayerController != nullptr)
		{
			PlayerController->EnterPatrolMode();
		}
		break;
	case EGP_ContextActionId::None:
	default:
		break;
	}
}

void UGP_ContextActionPresenter::RequestOpenMainBasePurchase()
{
	if (Mode != EGP_ContextActionMode::MainBase)
	{
		return;
	}

	SetPanelState(EGP_ContextActionPanelState::PurchaseRoot);
}

void UGP_ContextActionPresenter::RequestOpenPurchaseCategory(EGP_PurchaseCategory Category)
{
	if (Mode != EGP_ContextActionMode::MainBase
		|| PanelState != EGP_ContextActionPanelState::PurchaseRoot)
	{
		return;
	}

	SetPanelState(PanelStateForCategory(Category));
}

void UGP_ContextActionPresenter::RequestPurchaseBack()
{
	if (Mode != EGP_ContextActionMode::MainBase)
	{
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseBuildingSelected)
	{
		ClearSelectedPurchaseItem();
		SetPanelState(EGP_ContextActionPanelState::PurchaseBuildings);
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseDefenseSelected)
	{
		ClearSelectedPurchaseItem();
		SetPanelState(EGP_ContextActionPanelState::PurchaseDefense);
		return;
	}

	if (IsPurchaseCategoryState(PanelState))
	{
		SetPanelState(EGP_ContextActionPanelState::PurchaseRoot);
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseRoot)
	{
		SetPanelState(EGP_ContextActionPanelState::Actions);
	}
}

void UGP_ContextActionPresenter::SetPanelState(EGP_ContextActionPanelState NewState)
{
	if (PanelState == NewState)
	{
		return;
	}

	PanelState = NewState;
	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
}

bool UGP_ContextActionPresenter::IsPurchaseCategoryState(EGP_ContextActionPanelState State)
{
	return State == EGP_ContextActionPanelState::PurchaseUnits
		|| State == EGP_ContextActionPanelState::PurchaseBuildings
		|| State == EGP_ContextActionPanelState::PurchaseDefense;
}

bool UGP_ContextActionPresenter::IsPurchaseSelectedState(EGP_ContextActionPanelState State)
{
	return State == EGP_ContextActionPanelState::PurchaseBuildingSelected
		|| State == EGP_ContextActionPanelState::PurchaseDefenseSelected;
}

bool UGP_ContextActionPresenter::IsPurchasePresentationState(EGP_ContextActionPanelState State)
{
	return IsPurchaseCategoryState(State) || IsPurchaseSelectedState(State);
}

EGP_ContextActionPanelState UGP_ContextActionPresenter::PanelStateForCategory(
	EGP_PurchaseCategory Category)
{
	switch (Category)
	{
	case EGP_PurchaseCategory::Units:
		return EGP_ContextActionPanelState::PurchaseUnits;
	case EGP_PurchaseCategory::Buildings:
		return EGP_ContextActionPanelState::PurchaseBuildings;
	case EGP_PurchaseCategory::Defense:
		return EGP_ContextActionPanelState::PurchaseDefense;
	default:
		return EGP_ContextActionPanelState::PurchaseRoot;
	}
}

void UGP_ContextActionPresenter::ClearPurchaseSession()
{
	PendingUnitManifest = FGP_UnitDropManifest();
	ClearSelectedPurchaseItem();
	PendingAutoDeployItemId = FPrimaryAssetId();
	PendingAutoDeployReadyBefore = 0;
	PurchaseContextMessage = FText::GetEmpty();
}

void UGP_ContextActionPresenter::ClearSelectedPurchaseItem()
{
	SelectedPurchaseItemId = FPrimaryAssetId();
	SelectedPurchaseItemKind = EGP_PurchaseCatalogItemKind::Unit;
	SelectedPurchaseRow = FGP_PurchaseCatalogRow();
}

float UGP_ContextActionPresenter::GetLocalOrbitalFerronite() const
{
	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			if (const UGP_PlayerAttributeSet* Attributes = PlayerState->GetPlayerAttributeSet())
			{
				return Attributes->GetOrbitalFerronite();
			}
		}
	}
	return 0.0f;
}

int32 UGP_ContextActionPresenter::GetShuttleSlotCapacity()
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	return Settings != nullptr ? FMath::Max(1, Settings->PodTransportSlotCapacity) : 1;
}

FGP_PurchaseUnitManifestPresentation UGP_ContextActionPresenter::BuildUnitManifestPresentation() const
{
	FGP_PurchaseUnitManifestPresentation Out;
	Out.WorkerCount = FMath::Max(0, PendingUnitManifest.WorkerCount);
	Out.SalvageWalkerCount = FMath::Max(0, PendingUnitManifest.SalvageWalkerCount);
	Out.UnitCount = PendingUnitManifest.GetTotalUnitCount();
	Out.MaxSlots = GetShuttleSlotCapacity();

	if (Out.UnitCount <= 0)
	{
		Out.bCanLaunch = false;
		Out.DisabledReason = LOCTEXT("LaunchEmptyManifest", "Manifest is empty");
		return Out;
	}

	int32 SlotCost = 0;
	float OrbitalCost = 0.0f;
	int32 UnitCount = 0;
	EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
	if (!GPUnitDropAuthority::ComputeManifestCosts(
			PendingUnitManifest, SlotCost, OrbitalCost, UnitCount, Reject))
	{
		Out.UsedSlots = SlotCost;
		Out.TotalCost = OrbitalCost;
		Out.bCanLaunch = false;
		if (Reject == EGP_UnitDropRejectReason::SlotOverflow)
		{
			Out.DisabledReason = LOCTEXT("LaunchSlotOverflow", "Shuttle capacity reached");
		}
		else if (Reject == EGP_UnitDropRejectReason::DefinitionNotReady)
		{
			Out.DisabledReason = LOCTEXT("LaunchDefinitionNotReady", "DefinitionNotReady");
		}
		else
		{
			Out.DisabledReason = LOCTEXT("LaunchManifestInvalid", "Manifest is invalid");
		}
		return Out;
	}

	Out.UsedSlots = SlotCost;
	Out.TotalCost = OrbitalCost;
	if (GetLocalOrbitalFerronite() + KINDA_SMALL_NUMBER < OrbitalCost)
	{
		Out.bCanLaunch = false;
		Out.DisabledReason = LOCTEXT("LaunchInsufficientOrbital", "Not enough Orbital Ferronite");
		return Out;
	}

	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			if (!PlayerState->CanAcceptManifestUnitCount(Out.UnitCount))
			{
				Out.bCanLaunch = false;
				Out.DisabledReason = LOCTEXT("LaunchUnitCap", "Unit cap reached");
				return Out;
			}
		}
	}

	Out.bCanLaunch = Mode == EGP_ContextActionMode::MainBase
		&& PanelState == EGP_ContextActionPanelState::PurchaseUnits;
	return Out;
}

void UGP_ContextActionPresenter::RebuildSelectedPurchaseRow()
{
	using namespace GPPurchaseCatalogPresentationPrivate;

	SelectedPurchaseRow = FGP_PurchaseCatalogRow();
	if (!SelectedPurchaseItemId.IsValid())
	{
		return;
	}

	const float OrbitalFerronite = GetLocalOrbitalFerronite();
	if (SelectedPurchaseItemKind == EGP_PurchaseCatalogItemKind::WallPackage)
	{
		if (UGP_WallPackageCatalog* WallCatalog = UGP_WallPackageCatalog::Get())
		{
			if (UGP_WallPackageDefinition* Package = WallCatalog->GetWallPackage())
			{
				if (Package->GetPrimaryAssetId() == SelectedPurchaseItemId)
				{
					SelectedPurchaseRow.ItemId = Package->GetPrimaryAssetId();
					SelectedPurchaseRow.ItemKind = EGP_PurchaseCatalogItemKind::WallPackage;
					SelectedPurchaseRow.Category = EGP_PurchaseCategory::Defense;
					SelectedPurchaseRow.DisplayName = Package->DisplayName;
					SelectedPurchaseRow.Icon = ResolvePurchaseIcon(Package->Icon);
					SelectedPurchaseRow.Cost = Package->Cost;
					SelectedPurchaseRow.SegmentCount = Package->SegmentCount;
					FText ExtraReason;
					if (const UGP_WallSegmentInventoryComponent* Inventory = BoundWallInventory.Get())
					{
						if (Inventory->IsWallPackagePending())
						{
							ExtraReason = LOCTEXT("PurchaseWallDeliveryPending", "Delivery already pending");
						}
						else if (!Inventory->CanPurchaseWallPackage())
						{
							ExtraReason = LOCTEXT("PurchaseWallStockFull", "Wall stock full");
						}
					}
					ApplyAvailabilityGates(SelectedPurchaseRow, OrbitalFerronite, true, ExtraReason);
				}
			}
		}
		return;
	}

	UGP_BuildingDropCatalog& BuildingCatalog = UGP_BuildingDropCatalog::Get();
	if (UGP_OrbitalDropDefinition* Drop = BuildingCatalog.FindDropDefinition(SelectedPurchaseItemId))
	{
		const bool bReady = !BuildingCatalog.IsDropDefinitionPending(Drop)
			&& Drop->ResolveLoadedBuildingDefinition() != nullptr;
		UTexture2D* Icon = nullptr;
		if (const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition())
		{
			Icon = ResolveBuildingPurchaseIcon(Building);
		}
		SelectedPurchaseRow = MakeBuildingRow(
			Drop,
			SelectedPurchaseItemKind,
			SelectedPurchaseItemKind == EGP_PurchaseCatalogItemKind::DefensiveBuilding
				? EGP_PurchaseCategory::Defense
				: EGP_PurchaseCategory::Buildings,
			OrbitalFerronite,
			bReady,
			Icon);
	}
}

void UGP_ContextActionPresenter::RequestPurchaseRowPrimary(FPrimaryAssetId ItemId)
{
	if (Mode != EGP_ContextActionMode::MainBase || !ItemId.IsValid())
	{
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		TryAddUnitToManifest(ItemId);
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseBuildings
		|| PanelState == EGP_ContextActionPanelState::PurchaseDefense)
	{
		SelectPurchaseItem(ItemId);
	}
}

void UGP_ContextActionPresenter::RequestPurchaseRowSecondary(FPrimaryAssetId ItemId)
{
	if (Mode != EGP_ContextActionMode::MainBase || !ItemId.IsValid())
	{
		return;
	}

	if (PanelState == EGP_ContextActionPanelState::PurchaseUnits)
	{
		TryRemoveUnitFromManifest(ItemId);
	}
}

bool UGP_ContextActionPresenter::TryAddUnitToManifest(FPrimaryAssetId ItemId)
{
	UGP_OrbitalUnitDropCatalog& UnitCatalog = UGP_OrbitalUnitDropCatalog::Get();
	FGP_UnitDropManifest Next = PendingUnitManifest;
	bool bMatched = false;
	if (const UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitCatalog.GetWorkerDrop())
	{
		if (WorkerDrop->GetPrimaryAssetId() == ItemId)
		{
			Next.WorkerCount = FMath::Max(0, Next.WorkerCount) + 1;
			bMatched = true;
		}
	}
	if (!bMatched)
	{
		if (const UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitCatalog.GetSalvageWalkerDrop())
		{
			if (WalkerDrop->GetPrimaryAssetId() == ItemId)
			{
				Next.SalvageWalkerCount = FMath::Max(0, Next.SalvageWalkerCount) + 1;
				bMatched = true;
			}
		}
	}
	if (!bMatched)
	{
		return false;
	}

	int32 SlotCost = 0;
	float OrbitalCost = 0.0f;
	int32 UnitCount = 0;
	EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
	if (!GPUnitDropAuthority::ComputeManifestCosts(Next, SlotCost, OrbitalCost, UnitCount, Reject))
	{
		if (Reject == EGP_UnitDropRejectReason::SlotOverflow)
		{
			PurchaseContextMessage = LOCTEXT("AddSlotOverflow", "Shuttle capacity reached");
		}
		else
		{
			PurchaseContextMessage = LOCTEXT("AddDefinitionNotReady", "DefinitionNotReady");
		}
		RebuildPurchaseCatalogRows();
		OnContextActionsChanged.Broadcast();
		return false;
	}

	if (GetLocalOrbitalFerronite() + KINDA_SMALL_NUMBER < OrbitalCost)
	{
		PurchaseContextMessage = LOCTEXT("AddInsufficientOrbital", "Not enough Orbital Ferronite");
		RebuildPurchaseCatalogRows();
		OnContextActionsChanged.Broadcast();
		return false;
	}

	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			if (!PlayerState->CanAcceptManifestUnitCount(Next.GetTotalUnitCount()))
			{
				PurchaseContextMessage = LOCTEXT("AddUnitCap", "Unit cap reached");
				RebuildPurchaseCatalogRows();
				OnContextActionsChanged.Broadcast();
				return false;
			}
		}
	}

	PendingUnitManifest = Next;
	PurchaseContextMessage = FText::GetEmpty();
	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
	return true;
}

bool UGP_ContextActionPresenter::TryRemoveUnitFromManifest(FPrimaryAssetId ItemId)
{
	UGP_OrbitalUnitDropCatalog& UnitCatalog = UGP_OrbitalUnitDropCatalog::Get();
	bool bChanged = false;
	if (const UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitCatalog.GetWorkerDrop())
	{
		if (WorkerDrop->GetPrimaryAssetId() == ItemId && PendingUnitManifest.WorkerCount > 0)
		{
			--PendingUnitManifest.WorkerCount;
			bChanged = true;
		}
	}
	if (!bChanged)
	{
		if (const UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitCatalog.GetSalvageWalkerDrop())
		{
			if (WalkerDrop->GetPrimaryAssetId() == ItemId && PendingUnitManifest.SalvageWalkerCount > 0)
			{
				--PendingUnitManifest.SalvageWalkerCount;
				bChanged = true;
			}
		}
	}
	if (!bChanged)
	{
		return false;
	}

	PurchaseContextMessage = FText::GetEmpty();
	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
	return true;
}

void UGP_ContextActionPresenter::SelectPurchaseItem(FPrimaryAssetId ItemId)
{
	const FGP_PurchaseCatalogRow* Found = nullptr;
	for (const FGP_PurchaseCatalogRow& Row : PurchaseCatalogRows)
	{
		if (Row.ItemId == ItemId)
		{
			Found = &Row;
			break;
		}
	}
	if (Found == nullptr)
	{
		return;
	}

	SelectedPurchaseItemId = Found->ItemId;
	SelectedPurchaseItemKind = Found->ItemKind;
	if (PanelState == EGP_ContextActionPanelState::PurchaseBuildings
		&& Found->ItemKind == EGP_PurchaseCatalogItemKind::Building)
	{
		SetPanelState(EGP_ContextActionPanelState::PurchaseBuildingSelected);
		return;
	}
	if (PanelState == EGP_ContextActionPanelState::PurchaseDefense
		&& (Found->ItemKind == EGP_PurchaseCatalogItemKind::DefensiveBuilding
			|| Found->ItemKind == EGP_PurchaseCatalogItemKind::WallPackage))
	{
		SetPanelState(EGP_ContextActionPanelState::PurchaseDefenseSelected);
	}
}

void UGP_ContextActionPresenter::RequestLaunchUnitShuttle()
{
	if (Mode != EGP_ContextActionMode::MainBase
		|| PanelState != EGP_ContextActionPanelState::PurchaseUnits)
	{
		return;
	}

	const FGP_PurchaseUnitManifestPresentation Manifest = BuildUnitManifestPresentation();
	if (!Manifest.bCanLaunch || PendingUnitManifest.IsEmpty())
	{
		return;
	}

	AGP_PlayerController* PlayerController = BoundPlayerController.Get();
	if (PlayerController == nullptr)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	++DebugUnitDropRequestCount;
	DebugLastRequestedUnitManifest = PendingUnitManifest;
#endif
	PlayerController->RequestUnitDrop(PendingUnitManifest);
	PendingUnitManifest = FGP_UnitDropManifest();
	PurchaseContextMessage = FText::GetEmpty();
	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
}

void UGP_ContextActionPresenter::RequestLaunchSelectedPurchaseItem()
{
	if (Mode != EGP_ContextActionMode::MainBase || !IsPurchaseSelectedState(PanelState))
	{
		return;
	}

	RebuildSelectedPurchaseRow();
	if (!SelectedPurchaseRow.bEnabled || !SelectedPurchaseItemId.IsValid())
	{
		return;
	}

	LaunchSelectedBuildingOrDefense();
}

void UGP_ContextActionPresenter::LaunchSelectedBuildingOrDefense()
{
	AGP_PlayerController* PlayerController = BoundPlayerController.Get();
	if (PlayerController == nullptr)
	{
		return;
	}

	if (SelectedPurchaseItemKind == EGP_PurchaseCatalogItemKind::WallPackage)
	{
#if !UE_BUILD_SHIPPING
		++DebugWallPackagePurchaseRequestCount;
#endif
		PlayerController->RequestWallPackagePurchase();
		ClearSelectedPurchaseItem();
		SetPanelState(EGP_ContextActionPanelState::PurchaseDefense);
		return;
	}

	if (SelectedPurchaseItemKind != EGP_PurchaseCatalogItemKind::Building
		&& SelectedPurchaseItemKind != EGP_PurchaseCatalogItemKind::DefensiveBuilding)
	{
		return;
	}

	int32 ReadyBefore = 0;
	if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
	{
		if (const UGP_OrbitalBuildingInventoryComponent* Inventory =
				PlayerState->GetOrbitalBuildingInventoryComponent())
		{
			ReadyBefore = Inventory->GetReadyCount(SelectedPurchaseItemId);
		}
	}

	PendingAutoDeployItemId = SelectedPurchaseItemId;
	PendingAutoDeployReadyBefore = ReadyBefore;
#if !UE_BUILD_SHIPPING
	++DebugBuildingPurchaseRequestCount;
	DebugLastRequestedBuildingPurchaseId = SelectedPurchaseItemId;
#endif
	PlayerController->RequestBuildingPurchase(SelectedPurchaseItemId);
}

void UGP_ContextActionPresenter::EnsureBuildingInventoryBinding()
{
	UGP_OrbitalBuildingInventoryComponent* Inventory = nullptr;
	if (const AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			Inventory = PlayerState->GetOrbitalBuildingInventoryComponent();
		}
	}

	if (BoundBuildingInventory.Get() == Inventory && BuildingReadyChangedHandle.IsValid())
	{
		return;
	}

	UnbindBuildingInventory();
	if (!IsValid(Inventory))
	{
		return;
	}

	BoundBuildingInventory = Inventory;
	BuildingReadyChangedHandle = Inventory->OnReadyChanged.AddUObject(
		this, &ThisClass::HandleBuildingReadyChanged);
}

void UGP_ContextActionPresenter::UnbindBuildingInventory()
{
	if (UGP_OrbitalBuildingInventoryComponent* Inventory = BoundBuildingInventory.Get())
	{
		if (BuildingReadyChangedHandle.IsValid())
		{
			Inventory->OnReadyChanged.Remove(BuildingReadyChangedHandle);
		}
	}
	BuildingReadyChangedHandle.Reset();
	BoundBuildingInventory.Reset();
}

void UGP_ContextActionPresenter::HandleBuildingReadyChanged(FPrimaryAssetId DropDefinitionId, int32 NewCount)
{
	if (!PendingAutoDeployItemId.IsValid() || DropDefinitionId != PendingAutoDeployItemId)
	{
		RefreshPurchaseCatalogIfCategoryActive();
		return;
	}

	if (NewCount <= PendingAutoDeployReadyBefore)
	{
		return;
	}

	const FPrimaryAssetId DeployId = PendingAutoDeployItemId;
	PendingAutoDeployItemId = FPrimaryAssetId();
	PendingAutoDeployReadyBefore = 0;

	if (AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->EnterBuildingPlacementMode(DeployId);
	}
}

UTexture2D* UGP_ContextActionPresenter::FindResolvedPurchaseIcon(const TSoftObjectPtr<UTexture2D>& Soft) const
{
	using namespace GPPurchaseCatalogPresentationPrivate;

	if (Soft.IsNull())
	{
		return nullptr;
	}

	if (UTexture2D* Loaded = PeekLoadedTexture(Soft))
	{
		return Loaded;
	}

	const FSoftObjectPath Path = Soft.ToSoftObjectPath();
	if (const TObjectPtr<UTexture2D>* Cached = ResolvedPurchaseIcons.Find(Path))
	{
		if (IsValid(*Cached))
		{
			return Cached->Get();
		}
	}

	return nullptr;
}

UTexture2D* UGP_ContextActionPresenter::ResolvePurchaseIcon(const TSoftObjectPtr<UTexture2D>& Soft)
{
	if (Soft.IsNull())
	{
		return nullptr;
	}

	if (UTexture2D* Loaded = FindResolvedPurchaseIcon(Soft))
	{
		ResolvedPurchaseIcons.Add(Soft.ToSoftObjectPath(), Loaded);
		return Loaded;
	}

	RequestPurchaseIconLoad(Soft.ToSoftObjectPath());
	return nullptr;
}

UTexture2D* UGP_ContextActionPresenter::ResolveUnitPurchaseIcon(const UGP_OrbitalUnitDropDefinition* Drop)
{
	if (!IsValid(Drop))
	{
		return nullptr;
	}

	if (!Drop->Icon.IsNull())
	{
		if (UTexture2D* Override = FindResolvedPurchaseIcon(Drop->Icon))
		{
			ResolvedPurchaseIcons.Add(Drop->Icon.ToSoftObjectPath(), Override);
			return Override;
		}

		RequestPurchaseIconLoad(Drop->Icon.ToSoftObjectPath());
	}

	if (const UGP_UnitDefinition* UnitDef = Drop->ResolveLoadedUnitDefinition())
	{
		if (IsValid(UnitDef->PresentationIcon))
		{
			return UnitDef->PresentationIcon.Get();
		}
	}

	return nullptr;
}

UTexture2D* UGP_ContextActionPresenter::ResolveBuildingPurchaseIcon(const UGP_BuildingDefinition* Building)
{
	if (!IsValid(Building))
	{
		return nullptr;
	}

	if (!Building->Icon.IsNull())
	{
		if (UTexture2D* Override = FindResolvedPurchaseIcon(Building->Icon))
		{
			ResolvedPurchaseIcons.Add(Building->Icon.ToSoftObjectPath(), Override);
			return Override;
		}

		RequestPurchaseIconLoad(Building->Icon.ToSoftObjectPath());
	}

	if (const UGP_UnitDefinition* UnitDef = Building->ResolveLoadedUnitDefinition())
	{
		if (IsValid(UnitDef->PresentationIcon))
		{
			return UnitDef->PresentationIcon.Get();
		}
	}

	return nullptr;
}

void UGP_ContextActionPresenter::RequestPurchaseIconLoad(const FSoftObjectPath& Path)
{
	if (bPurchaseIconLoadsAbandoned || !Path.IsValid())
	{
		return;
	}
	if (PendingPurchaseIconLoads.Contains(Path) || PurchaseIconLoadAttempted.Contains(Path))
	{
		return;
	}

	PurchaseIconLoadAttempted.Add(Path);

#if !UE_BUILD_SHIPPING
	++DebugPurchaseIconRequestCount;
	if (bDebugHoldPurchaseIconCompletion)
	{
		PendingPurchaseIconLoads.Add(Path, nullptr);
		return;
	}
#endif

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Path,
		FStreamableDelegate::CreateUObject(this, &UGP_ContextActionPresenter::HandlePurchaseIconLoaded, Path));
	PendingPurchaseIconLoads.Add(Path, Handle);
	if (!Handle.IsValid())
	{
		PendingPurchaseIconLoads.Remove(Path);
	}
}

void UGP_ContextActionPresenter::HandlePurchaseIconLoaded(FSoftObjectPath Path)
{
	if (bPurchaseIconLoadsAbandoned || !IsValid(this))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (bDebugHoldPurchaseIconCompletion)
	{
		return;
	}
#endif

	TSharedPtr<FStreamableHandle> Handle;
	PendingPurchaseIconLoads.RemoveAndCopyValue(Path, Handle);
	UTexture2D* Loaded = nullptr;
	if (Handle.IsValid())
	{
		Loaded = Cast<UTexture2D>(Handle->GetLoadedAsset());
	}
	if (Loaded == nullptr)
	{
		Loaded = Cast<UTexture2D>(Path.ResolveObject());
	}
	if (IsValid(Loaded))
	{
		ResolvedPurchaseIcons.Add(Path, Loaded);
	}

	if (bInsidePurchaseCatalogRebuild)
	{
		bPurchaseIconRefreshDeferred = true;
		return;
	}

	RefreshPurchaseCatalogIfCategoryActive();
}

void UGP_ContextActionPresenter::CancelPurchaseIconLoads()
{
	for (TPair<FSoftObjectPath, TSharedPtr<FStreamableHandle>>& Pair : PendingPurchaseIconLoads)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->CancelHandle();
			Pair.Value.Reset();
		}
	}
	PendingPurchaseIconLoads.Reset();
	ResolvedPurchaseIcons.Reset();
	PurchaseIconLoadAttempted.Reset();
}

#if !UE_BUILD_SHIPPING
void UGP_ContextActionPresenter::DebugHoldPurchaseIconCompletion(bool bHold)
{
	bDebugHoldPurchaseIconCompletion = bHold;
}

void UGP_ContextActionPresenter::DebugInjectHeldPurchaseIcon(const FSoftObjectPath& Path, UTexture2D* Texture)
{
	if (Path.IsValid() && IsValid(Texture))
	{
		DebugHeldPurchaseIcons.Add(Path, Texture);
	}
}

void UGP_ContextActionPresenter::DebugCompleteHeldPurchaseIconLoad(const FSoftObjectPath& Path)
{
	if (!Path.IsValid() || bPurchaseIconLoadsAbandoned || !PendingPurchaseIconLoads.Contains(Path))
	{
		return;
	}

	if (TObjectPtr<UTexture2D>* Held = DebugHeldPurchaseIcons.Find(Path))
	{
		if (IsValid(*Held))
		{
			ResolvedPurchaseIcons.Add(Path, *Held);
		}
	}
	PendingPurchaseIconLoads.Remove(Path);

	if (bInsidePurchaseCatalogRebuild)
	{
		bPurchaseIconRefreshDeferred = true;
		return;
	}

	RefreshPurchaseCatalogIfCategoryActive();
}

void UGP_ContextActionPresenter::DebugCancelPurchaseIconLoads()
{
	const bool bWasAbandoned = bPurchaseIconLoadsAbandoned;
	bPurchaseIconLoadsAbandoned = true;
	CancelPurchaseIconLoads();
	bPurchaseIconLoadsAbandoned = bWasAbandoned;
}
#endif

#undef LOCTEXT_NAMESPACE
