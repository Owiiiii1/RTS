// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPLaunchMenuPresenter.generated.h"

class AGP_GameState;
class AGP_MainBase;
class UGP_StorageComponent;

USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_LaunchContainerRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|LaunchMenu")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|LaunchMenu")
	float StoredAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|LaunchMenu")
	float Capacity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|LaunchMenu")
	float FillNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|LaunchMenu")
	bool bIsReadyForLaunch = false;
};

DECLARE_MULTICAST_DELEGATE(FOnGPLaunchMenuPresentationChanged);

/**
 * LocalPlayer-owned, event-driven launch-menu presentation.
 * Reads local-team MainBase storage only. Does not tick or scan the world.
 */
UCLASS()
class GPUIRUNTIME_API UGP_LaunchMenuPresenter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(AGP_GameState* InGameState, int32 InLocalTeamId);
	void Shutdown();

	const TArray<FGP_LaunchContainerRow>& GetRows() const { return Rows; }
	bool CanLaunchReadyContainer() const { return bCanLaunchReadyContainer; }
	int32 GetReadyLaunchContainerCount() const { return ReadyLaunchContainerCount; }
	int32 GetBoundDelegateCount() const;

	FOnGPLaunchMenuPresentationChanged OnLaunchMenuPresentationChanged;

protected:
	virtual void BeginDestroy() override;

private:
	void BindLocalMainBaseStorage();
	void UnbindLocalMainBaseStorage();
	void RebuildPresentation();
	void HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* Previous, AGP_MainBase* NewBase);

	UFUNCTION()
	void HandleStorageChanged(float PreviousTotalStored, float NewTotalStored, float TotalCapacity);

	TWeakObjectPtr<AGP_GameState> BoundGameState;
	TWeakObjectPtr<UGP_StorageComponent> BoundStorage;
	int32 LocalTeamId = -1;
	FDelegateHandle ResolvedMainBaseHandle;
	TArray<FGP_LaunchContainerRow> Rows;
	int32 ReadyLaunchContainerCount = 0;
	bool bCanLaunchReadyContainer = false;
};
