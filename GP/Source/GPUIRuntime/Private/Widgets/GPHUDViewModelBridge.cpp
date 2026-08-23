// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPHUDViewModelBridge.h"

#include "MVVMSubsystem.h"
#include "View/MVVMView.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMatchViewModel.h"
#include "ViewModels/GPResourceViewModel.h"
#include "ViewModels/GPSelectionViewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDViewModelBridge, Log, All);

const FName FGP_HUDViewModelBridge::ResourceViewModelSlotName(TEXT("GP_ResourceViewModel"));
const FName FGP_HUDViewModelBridge::MatchViewModelSlotName(TEXT("GP_MatchViewModel"));
const FName FGP_HUDViewModelBridge::SelectionViewModelSlotName(TEXT("GP_SelectionViewModel"));

FGP_HUDViewModelBridgeResult FGP_HUDViewModelBridge::AssignOwnedViewModels(
	UUserWidget* Widget,
	UGP_HUDViewModelSubsystem* Subsystem)
{
	FGP_HUDViewModelBridgeResult Result;
	Result.bHadSubsystem = Subsystem != nullptr;
	if (Subsystem == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: missing UGP_HUDViewModelSubsystem"));
#endif
		return Result;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	Result.bHadView = View != nullptr;
	if (View == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: missing UMVVMView on widget"));
#endif
		return Result;
	}

	return AssignOwnedViewModelsToView(View, Subsystem);
}

FGP_HUDViewModelBridgeResult FGP_HUDViewModelBridge::AssignOwnedViewModelsToView(
	UMVVMView* View,
	UGP_HUDViewModelSubsystem* Subsystem)
{
	FGP_HUDViewModelBridgeResult Result;
	Result.bHadView = View != nullptr;
	Result.bHadSubsystem = Subsystem != nullptr;
	if (View == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: missing UMVVMView"));
#endif
		return Result;
	}

	if (Subsystem == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: missing UGP_HUDViewModelSubsystem"));
#endif
		return Result;
	}

	UGP_ResourceViewModel* ResourceViewModel = Subsystem->GetResourceViewModel();
	UGP_MatchViewModel* MatchViewModel = Subsystem->GetMatchViewModel();
	UGP_SelectionViewModel* SelectionViewModel = Subsystem->GetSelectionViewModel();
	if (ResourceViewModel == nullptr && MatchViewModel == nullptr && SelectionViewModel == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: subsystem ViewModels are null; not creating replacements"));
#endif
		return Result;
	}

	if (ResourceViewModel != nullptr)
	{
		Result.bResourceAssigned = View->SetViewModel(ResourceViewModelSlotName, ResourceViewModel);
	}
	if (MatchViewModel != nullptr)
	{
		Result.bMatchAssigned = View->SetViewModel(MatchViewModelSlotName, MatchViewModel);
	}
	if (SelectionViewModel != nullptr)
	{
		Result.bSelectionAssigned = View->SetViewModel(SelectionViewModelSlotName, SelectionViewModel);
	}

#if !UE_BUILD_SHIPPING
	if (ResourceViewModel != nullptr && !Result.bResourceAssigned)
	{
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: failed to assign slot '%s'"),
			*ResourceViewModelSlotName.ToString());
	}
	if (MatchViewModel != nullptr && !Result.bMatchAssigned)
	{
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: failed to assign slot '%s'"),
			*MatchViewModelSlotName.ToString());
	}
	if (SelectionViewModel != nullptr && !Result.bSelectionAssigned)
	{
		UE_LOG(LogGPHUDViewModelBridge, Warning,
			TEXT("HUD ViewModel bridge: failed to assign slot '%s'"),
			*SelectionViewModelSlotName.ToString());
	}
#endif

	return Result;
}
