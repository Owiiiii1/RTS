// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GPMatchViewModelAdapter.generated.h"

class AGP_GameState;
class UGP_MatchViewModel;

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

protected:
	virtual void BeginDestroy() override;

private:
	void RefreshSnapshot();
	void HandleMatchStateChanged(FGameplayTag OldTag, FGameplayTag NewTag);
	void HandleMatchTimeChanged(float OldTime, float NewTime);
	void HandleTeamThreatChanged(int32 TeamId, float OldThreat, float NewThreat);
	void HandleMatchResultChanged(
		int32 OldWinnerTeamId,
		int32 NewWinnerTeamId,
		FGameplayTag OldWinReasonTag,
		FGameplayTag NewWinReasonTag);

	UPROPERTY(Transient)
	TObjectPtr<UGP_MatchViewModel> ViewModel;

	TWeakObjectPtr<AGP_GameState> BoundGameState;
	int32 LocalTeamId = -1;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle MatchTimeHandle;
	FDelegateHandle TeamThreatHandle;
	FDelegateHandle MatchResultHandle;
};
