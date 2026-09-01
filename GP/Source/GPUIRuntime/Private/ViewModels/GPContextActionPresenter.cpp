// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPContextActionPresenter.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Engine/Texture2D.h"
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
#include "Units/GPUnitBase.h"

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

	static UTexture2D* ResolveLoadedTexture(const TSoftObjectPtr<UTexture2D>& Soft)
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
		bool bProductReady)
	{
		FGP_PurchaseCatalogRow Row;
		Row.ItemId = Drop->GetPrimaryAssetId();
		Row.ItemKind = ItemKind;
		Row.Category = Category;
		Row.DisplayName = Drop->GetAcquisitionDisplayName();
		if (const UGP_BuildingDefinition* Building = Drop->ResolveLoadedBuildingDefinition())
		{
			Row.Icon = ResolveLoadedTexture(Building->Icon);
		}
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
	SelectionChangedHandle = Selection->OnSelectionChanged().AddUObject(
		this, &ThisClass::HandleSelectionChanged);
	CommandTargetingChangedHandle = InPlayerController->OnCommandTargetingModeChanged().AddUObject(
		this, &ThisClass::HandleCommandTargetingModeChanged);
	RebuildPresentation();
	return true;
}

void UGP_ContextActionPresenter::Shutdown()
{
	UnbindWallInventory();
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
	Count += OrbitalFerroniteHandle.IsValid() ? 1 : 0;
	Count += BoundWallInventory.IsValid() ? 2 : 0;
	for (const FBoundSelectedUnit& Bound : BoundUnits)
	{
		Count += Bound.DiedHandle.IsValid() ? 1 : 0;
		Count += Bound.Unit.IsValid() ? 1 : 0;
	}
	return Count;
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
	if (!IsPurchaseCategoryState(PanelState))
	{
		return;
	}

	RebuildPurchaseCatalogRows();
	OnContextActionsChanged.Broadcast();
}

void UGP_ContextActionPresenter::RebuildPurchaseCatalogRows()
{
	using namespace GPPurchaseCatalogPresentationPrivate;

	PurchaseCatalogRows.Reset();
	if (Mode != EGP_ContextActionMode::MainBase || !IsPurchaseCategoryState(PanelState))
	{
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
			Row.Icon = ResolveLoadedTexture(Drop->Icon);
			Row.Cost = Drop->Cost;
			Row.TransportSlotCost = Drop->TransportSlotCost;
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
			PurchaseCatalogRows.Add(MakeBuildingRow(
				Drop,
				EGP_PurchaseCatalogItemKind::Building,
				EGP_PurchaseCategory::Buildings,
				OrbitalFerronite,
				bReady));
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
			PurchaseCatalogRows.Add(MakeBuildingRow(
				Drop,
				EGP_PurchaseCatalogItemKind::DefensiveBuilding,
				EGP_PurchaseCategory::Defense,
				OrbitalFerronite,
				bReady));
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
				Row.Icon = ResolveLoadedTexture(Package->Icon);
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

#undef LOCTEXT_NAMESPACE
