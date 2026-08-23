// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPHUDRootWidget.h"

#include "Engine/LocalPlayer.h"
#include "Player/GPPlayerController.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPLaunchMenuPresenter.h"
#include "Widgets/GPHUDViewModelBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDRootWidget, Log, All);

void UGP_HUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryAssignOwnedViewModels();
	BindLaunchMenuPresenter();
}

void UGP_HUDRootWidget::NativeDestruct()
{
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
