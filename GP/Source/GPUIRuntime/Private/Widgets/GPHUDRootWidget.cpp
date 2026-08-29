// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPHUDRootWidget.h"

#include "Engine/LocalPlayer.h"
#include "Player/GPPlayerController.h"
#include "Player/GPSelectionComponent.h"
#include "Units/GPUnitBase.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPContextActionPresenter.h"
#include "ViewModels/GPLaunchMenuPresenter.h"
#include "ViewModels/GPSelectionViewModel.h"
#include "Widgets/GPHUDViewModelBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDRootWidget, Log, All);

void UGP_HUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryAssignOwnedViewModels();
	BindLaunchMenuPresenter();
	BindSelectionViewModel();
	BindContextActionPresenter();
}

void UGP_HUDRootWidget::NativeDestruct()
{
	UnbindContextActionPresenter();
	UnbindSelectionViewModel();
	UnbindLaunchMenuPresenter();
	Super::NativeDestruct();
}

void UGP_HUDRootWidget::TryAssignOwnedViewModels()
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (LocalPlayer == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDRootWidget, Warning,
			TEXT("UGP_HUDRootWidget: no owning LocalPlayer; skipping ViewModel bridge"));
#endif
		return;
	}

	UGP_HUDViewModelSubsystem* Subsystem = LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>();
	FGP_HUDViewModelBridge::AssignOwnedViewModels(this, Subsystem);
}

void UGP_HUDRootWidget::BindLaunchMenuPresenter()
{
	UnbindLaunchMenuPresenter();
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UGP_HUDViewModelSubsystem* Subsystem =
		LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
	UGP_LaunchMenuPresenter* Presenter =
		Subsystem != nullptr ? Subsystem->GetLaunchMenuPresenter() : nullptr;
	if (Presenter == nullptr)
	{
		return;
	}

	BoundLaunchMenuPresenter = Presenter;
	Presenter->OnLaunchMenuPresentationChanged.AddUObject(
		this, &ThisClass::HandleLaunchMenuPresentationChanged);
	HandleLaunchMenuPresentationChanged();
}

void UGP_HUDRootWidget::UnbindLaunchMenuPresenter()
{
	if (UGP_LaunchMenuPresenter* Presenter = BoundLaunchMenuPresenter.Get())
	{
		Presenter->OnLaunchMenuPresentationChanged.RemoveAll(this);
	}
	BoundLaunchMenuPresenter.Reset();
}

void UGP_HUDRootWidget::HandleLaunchMenuPresentationChanged()
{
	BP_OnLaunchMenuChanged();
}

const UGP_LaunchMenuPresenter* UGP_HUDRootWidget::ResolveLaunchMenuPresenter() const
{
	if (const UGP_LaunchMenuPresenter* Bound = BoundLaunchMenuPresenter.Get())
	{
		return Bound;
	}
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UGP_HUDViewModelSubsystem* Subsystem =
		LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
	return Subsystem != nullptr ? Subsystem->GetLaunchMenuPresenter() : nullptr;
}

TArray<FGP_LaunchContainerRow> UGP_HUDRootWidget::GetLaunchContainerRows() const
{
	if (const UGP_LaunchMenuPresenter* Presenter = ResolveLaunchMenuPresenter())
	{
		return Presenter->GetRows();
	}
	return TArray<FGP_LaunchContainerRow>();
}

TArray<FGP_LaunchContainerRow> UGP_HUDRootWidget::GetLaunchContainerPresentations() const
{
	return GetLaunchContainerRows();
}

bool UGP_HUDRootWidget::CanLaunchReadyContainer() const
{
	const UGP_LaunchMenuPresenter* Presenter = ResolveLaunchMenuPresenter();
	return Presenter != nullptr && Presenter->CanLaunchReadyContainer();
}

int32 UGP_HUDRootWidget::GetReadyLaunchContainerCount() const
{
	const UGP_LaunchMenuPresenter* Presenter = ResolveLaunchMenuPresenter();
	return Presenter != nullptr ? Presenter->GetReadyLaunchContainerCount() : 0;
}

void UGP_HUDRootWidget::RequestLaunchReadyContainer()
{
	if (AGP_PlayerController* PlayerController = Cast<AGP_PlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestLaunchReadyContainer();
	}
}

void UGP_HUDRootWidget::BindSelectionViewModel()
{
	UnbindSelectionViewModel();
	const UGP_SelectionViewModel* SelectionVM = ResolveSelectionViewModel();
	UGP_SelectionViewModel* MutableSelectionVM = const_cast<UGP_SelectionViewModel*>(SelectionVM);
	if (MutableSelectionVM == nullptr)
	{
		return;
	}

	BoundSelectionViewModel = MutableSelectionVM;
	MutableSelectionVM->OnSelectionPresentationChanged.AddUObject(
		this, &ThisClass::HandleSelectionPresentationChanged);
	HandleSelectionPresentationChanged();
}

void UGP_HUDRootWidget::UnbindSelectionViewModel()
{
	if (UGP_SelectionViewModel* SelectionVM = BoundSelectionViewModel.Get())
	{
		SelectionVM->OnSelectionPresentationChanged.RemoveAll(this);
	}
	BoundSelectionViewModel.Reset();
}

void UGP_HUDRootWidget::HandleSelectionPresentationChanged()
{
	BP_OnSelectionPresentationChanged();
}

const UGP_SelectionViewModel* UGP_HUDRootWidget::ResolveSelectionViewModel() const
{
	if (const UGP_SelectionViewModel* Bound = BoundSelectionViewModel.Get())
	{
		return Bound;
	}
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UGP_HUDViewModelSubsystem* Subsystem =
		LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
	return Subsystem != nullptr ? Subsystem->GetSelectionViewModel() : nullptr;
}

TArray<FGP_SelectionGroupRow> UGP_HUDRootWidget::GetSelectionGroupRows() const
{
	if (const UGP_SelectionViewModel* SelectionVM = ResolveSelectionViewModel())
	{
		return SelectionVM->GetGroupRows();
	}
	return TArray<FGP_SelectionGroupRow>();
}

void UGP_HUDRootWidget::RequestSelectGroupRow(int32 RowIndex)
{
	if (RowIndex < 0)
	{
		return;
	}

	AGP_PlayerController* PlayerController = Cast<AGP_PlayerController>(GetOwningPlayer());
	UGP_SelectionComponent* Selection =
		PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
	if (Selection == nullptr)
	{
		return;
	}

	TArray<AGP_UnitBase*> LiveUnits;
	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : Selection->GetSelectedUnits())
	{
		AGP_UnitBase* Unit = WeakUnit.Get();
		if (IsValid(Unit) && !Unit->IsActorBeingDestroyed() && !Unit->IsDead())
		{
			LiveUnits.Add(Unit);
		}
	}

	if (LiveUnits.Num() < 2 || !LiveUnits.IsValidIndex(RowIndex))
	{
		return;
	}

	AGP_UnitBase* Unit = LiveUnits[RowIndex];
	if (!IsValid(Unit) || Unit->IsActorBeingDestroyed() || !Unit->IsGameplaySelectable())
	{
		return;
	}

	Selection->ReplaceSelectionWithUnit(Unit);
}

void UGP_HUDRootWidget::BindContextActionPresenter()
{
	UnbindContextActionPresenter();
	const UGP_ContextActionPresenter* Presenter = ResolveContextActionPresenter();
	UGP_ContextActionPresenter* MutablePresenter = const_cast<UGP_ContextActionPresenter*>(Presenter);
	if (MutablePresenter == nullptr)
	{
		return;
	}

	BoundContextActionPresenter = MutablePresenter;
	MutablePresenter->OnContextActionsChanged.AddUObject(
		this, &ThisClass::HandleContextActionsChanged);
	HandleContextActionsChanged();
}

void UGP_HUDRootWidget::UnbindContextActionPresenter()
{
	if (UGP_ContextActionPresenter* Presenter = BoundContextActionPresenter.Get())
	{
		Presenter->OnContextActionsChanged.RemoveAll(this);
	}
	BoundContextActionPresenter.Reset();
}

void UGP_HUDRootWidget::HandleContextActionsChanged()
{
	BP_OnContextActionsChanged();
}

const UGP_ContextActionPresenter* UGP_HUDRootWidget::ResolveContextActionPresenter() const
{
	if (const UGP_ContextActionPresenter* Bound = BoundContextActionPresenter.Get())
	{
		return Bound;
	}
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UGP_HUDViewModelSubsystem* Subsystem =
		LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
	return Subsystem != nullptr ? Subsystem->GetContextActionPresenter() : nullptr;
}

TArray<FGP_ContextActionPresentation> UGP_HUDRootWidget::GetContextActionPresentations() const
{
	if (const UGP_ContextActionPresenter* Presenter = ResolveContextActionPresenter())
	{
		return Presenter->GetActions();
	}
	return TArray<FGP_ContextActionPresentation>();
}

EGP_ContextActionMode UGP_HUDRootWidget::GetContextActionMode() const
{
	if (const UGP_ContextActionPresenter* Presenter = ResolveContextActionPresenter())
	{
		return Presenter->GetMode();
	}
	return EGP_ContextActionMode::None;
}

EGP_ContextActionPanelState UGP_HUDRootWidget::GetContextActionPanelState() const
{
	if (const UGP_ContextActionPresenter* Presenter = ResolveContextActionPresenter())
	{
		return Presenter->GetPanelState();
	}
	return EGP_ContextActionPanelState::Actions;
}

FText UGP_HUDRootWidget::GetCommandTargetingPrompt() const
{
	if (const UGP_ContextActionPresenter* Presenter = ResolveContextActionPresenter())
	{
		return Presenter->GetTargetingPrompt();
	}
	return FText::GetEmpty();
}

void UGP_HUDRootWidget::RequestContextAction(EGP_ContextActionId ActionId)
{
	UGP_ContextActionPresenter* Presenter =
		const_cast<UGP_ContextActionPresenter*>(ResolveContextActionPresenter());
	if (Presenter != nullptr)
	{
		Presenter->RequestContextAction(ActionId);
	}
}

void UGP_HUDRootWidget::RequestOpenMainBasePurchase()
{
	UGP_ContextActionPresenter* Presenter =
		const_cast<UGP_ContextActionPresenter*>(ResolveContextActionPresenter());
	if (Presenter != nullptr)
	{
		Presenter->RequestOpenMainBasePurchase();
	}
}
