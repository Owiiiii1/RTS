// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "GPGameMode.generated.h"

class AGP_GameState;

/**
 * Server-only match flow orchestrator.
 * Owns the 1 Hz countdown timer; writes state through AGP_GameState.
 * Does not use engine MatchState / StartMatch / EndMatch as project SoT.
 */
UCLASS()
class GPRUNTIME_API AGP_GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGP_GameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void TryStartMatch();
	void StartMatchFlow();
	void StopMatchCountdown();
	void HandleMatchCountdownTick();
	void HandleMatchTimeExpired();

	void FinishMatch(int32 InWinnerTeamId, FGameplayTag InWinReasonTag);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "0.0"))
	float MatchDurationSeconds = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 ExpectedHumanPlayers = 2;

	virtual void EvaluateAndFinishMatch();
	virtual void OnMatchFlowStarted();
	virtual void OnMatchFlowFinished(int32 WinnerTeamId, FGameplayTag WinReasonTag);

	AGP_GameState* GetGPGameState() const;
	int32 GetConnectedHumanPlayerCount() const;

	static bool IsWinReasonBranchTag(const FGameplayTag& Tag);

private:
	/** Server-only monotonic allocator for playable TeamIds (starts at 1). Not replicated. */
	void AssignPlayableTeamId(APlayerController* NewPlayer);

	FTimerHandle MatchCountdownHandle;
	bool bTimeoutEvaluationTriggered = false;

	/** Next playable TeamId to assign (1+). Never reused; not decremented on logout. */
	int32 NextPlayableTeamId = 1;
};
