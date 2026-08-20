// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPFoWViewModelAdapter.generated.h"

class AGP_PlayerController;
class UGP_FoWViewModel;
class UGP_LocalFoWComponent;

/**
 * Push-based local adapter. A future production HUD root owns one adapter per local player.
 * It never scans the world and never ticks.
 */
UCLASS(BlueprintType)
class GPUIRUNTIME_API UGP_FoWViewModelAdapter : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GP|FogOfWar|MVVM")
	static UGP_FoWViewModelAdapter* CreateForPlayerController(AGP_PlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|MVVM")
	UGP_FoWViewModel* GetViewModel() const { return ViewModel; }

	bool InitializeWithMirror(UGP_LocalFoWComponent* Mirror);
	void Shutdown();

protected:
	virtual void BeginDestroy() override;

private:
	void HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror);

	UPROPERTY(Transient)
	TObjectPtr<UGP_FoWViewModel> ViewModel;

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;
};
