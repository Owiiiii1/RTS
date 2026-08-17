// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPMatchWinLoseContractTest.generated.h"

/** GP-S34W match win/lose contract runner. */
UCLASS()
class GPRUNTIME_API UGP_MatchWinLoseContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

private:
	void ScheduleNext(float DelaySeconds = 0.05f);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();
	void RestoreLiveTeams();
	bool ResetAndStartMatch();
	bool IsolateLivePlayableTeams();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_PlayerState> TeamAStateWeak;
	TWeakObjectPtr<class AGP_PlayerState> TeamBStateWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseAWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseBWeak;
	TWeakObjectPtr<class AGP_UnitBase> ExtraUnitWeak;
	TWeakObjectPtr<class AGP_LogisticsHub> ExtraHubWeak;
	TArray<TWeakObjectPtr<class AGP_PlayerState>> SavedLivePlayerStates;
	TArray<int32> SavedLiveTeamIds;
	int32 FirstSeedWinner = -1;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
	bool bIsolatedLiveTeams = false;
};
