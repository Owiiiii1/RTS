// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GPMatchViewModelAdapter.generated.h"

class AGP_GameState;
class AGP_MainBase;
class UGP_MatchViewModel;
class UGP_StorageComponent;

/** Push-only AGP_GameState adapter scoped to one local player's team. */
UCLASS()
class GPUIRUNTIME_API UGP_MatchViewModelAdapter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(UGP_MatchViewModel* InViewModel, AGP_GameState* InGameState, int32 InLocalTeamId);
	void Shutdown();

	UGP_MatchViewModel* GetViewModel() const { return ViewModel; }
	int32 GetBoundDelegateCount() const;

	static float ComputeThreatPresentationMax(float TotalCapacity, float ThreatPerStoredUnit);
	static float ComputeFerroniteThreatNormalized(
		float ThreatValue,
		float TotalCapacity,
		float ThreatPerStoredUnit);
	float GetThreatPresentationMax() const;

protected:
	virtual void BeginDestroy() override;

private:
	void RefreshSnapshot();
	void RefreshThreatPresentation();
	void BindLocalMainBaseStorage();
	void UnbindLocalMainBaseStorage();
	void HandleMatchStateChanged(FGameplayTag OldTag, FGameplayTag NewTag);
	void HandleMatchTimeChanged(float OldTime, float NewTime);
	void HandleTeamThreatChanged(int32 TeamId, float OldThreat, float NewThreat);
	void HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* Previous, AGP_MainBase* NewBase);
	void HandleMatchResultChanged(
		int32 OldWinnerTeamId,
		int32 NewWinnerTeamId,
		FGameplayTag OldWinReasonTag,
		FGameplayTag NewWinReasonTag);

	UFUNCTION()
	void HandleStorageChanged(float PreviousTotalStored, float NewTotalStored, float TotalCapacity);

	UPROPERTY(Transient)
	TObjectPtr<UGP_MatchViewModel> ViewModel;

	TWeakObjectPtr<AGP_GameState> BoundGameState;
	TWeakObjectPtr<UGP_StorageComponent> BoundStorage;
	int32 LocalTeamId = -1;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle MatchTimeHandle;
	FDelegateHandle TeamThreatHandle;
	FDelegateHandle MatchResultHandle;
	FDelegateHandle ResolvedMainBaseHandle;
};
