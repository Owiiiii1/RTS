// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPContextActionPresenter.h"

#include "Buildings/GPMainBase.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Units/GPUnitBase.h"

#define LOCTEXT_NAMESPACE "GPContextActions"

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

void UGP_ContextActionPresenter::RebuildPresentation()
{
	UnbindSelectedUnits();

	TArray<AGP_UnitBase*> LiveSelected;
	CollectLiveSelectedUnits(BoundSelection.Get(), LiveSelected);
	BindSelectedUnits(LiveSelected);

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
