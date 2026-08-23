// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UGP_HUDViewModelSubsystem;
class UMVVMView;
class UUserWidget;

/** Result of assigning subsystem-owned HUD ViewModels into an authored MVVM View. */
struct FGP_HUDViewModelBridgeResult
{
	bool bHadView = false;
	bool bHadSubsystem = false;
	bool bResourceAssigned = false;
	bool bMatchAssigned = false;
	bool bSelectionAssigned = false;
};

/**
 * Runtime bridge from UGP_HUDViewModelSubsystem into authored Manual MVVM slots.
 * Does not create ViewModels. The subsystem remains the lifetime owner.
 */
struct GPUIRUNTIME_API FGP_HUDViewModelBridge
{
	/** Authored WBP_GP_HUD Manual MVVM entry names. */
	static const FName ResourceViewModelSlotName;
	static const FName MatchViewModelSlotName;
	static const FName SelectionViewModelSlotName;

	static FGP_HUDViewModelBridgeResult AssignOwnedViewModels(
		UUserWidget* Widget,
		UGP_HUDViewModelSubsystem* Subsystem);

	static FGP_HUDViewModelBridgeResult AssignOwnedViewModelsToView(
		UMVVMView* View,
		UGP_HUDViewModelSubsystem* Subsystem);
};
