// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/GPLaunchMenuPresenter.h"
#include "Widgets/GPUserWidgetBase.h"
#include "GPHUDRootWidget.generated.h"

class UGP_LaunchMenuPresenter;

/** Native lifetime root for the authored WBP_GP_HUD production HUD. */
UCLASS(Abstract, Blueprintable)
class GPUIRUNTIME_API UGP_HUDRootWidget : public UGP_UserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	TArray<FGP_LaunchContainerRow> GetLaunchContainerRows() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	TArray<FGP_LaunchContainerRow> GetLaunchContainerPresentations() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	bool CanLaunchReadyContainer() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	int32 GetReadyLaunchContainerCount() const;

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|LaunchMenu")
	void RequestLaunchReadyContainer();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|HUD|LaunchMenu")
	void BP_OnLaunchMenuChanged();

private:
	void TryAssignOwnedViewModels();
	void BindLaunchMenuPresenter();
	void UnbindLaunchMenuPresenter();
	void HandleLaunchMenuPresentationChanged();
	const UGP_LaunchMenuPresenter* ResolveLaunchMenuPresenter() const;

	TWeakObjectPtr<UGP_LaunchMenuPresenter> BoundLaunchMenuPresenter;
};
