// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPHUDRootWidget.h"

#include "Engine/LocalPlayer.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "Widgets/GPHUDViewModelBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDRootWidget, Log, All);

void UGP_HUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryAssignOwnedViewModels();
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
