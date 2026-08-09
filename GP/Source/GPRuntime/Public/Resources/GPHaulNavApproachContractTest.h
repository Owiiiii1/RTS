// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPHaulNavApproachContractTest.generated.h"

/**
 * GP-S33M haul nav-aware approach contract:
 * MainBase NavigationObstacle active; direct radial candidate forced unavailable;
 * alternate reachable candidate selected; unload succeeds; all-unreachable → WaitingForDropOff.
 */
UCLASS()
class GPRUNTIME_API UGP_HaulNavApproachContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag)
	{
		ExecutionId = InExecutionId;
		OwnerTag = InOwnerTag;
	}
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

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerWeak;
	TWeakObjectPtr<class AGP_ResourceNode> NodeWeak;
	FVector MainBaseLocation = FVector::ZeroVector;
	FVector WorkerSpawnLocation = FVector::ZeroVector;
	int32 ContractTeamId = 1;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = 0.0;
	float MovementWaitTimeoutSeconds = 45.0f;
	int32 SelectedCandidateIndex = -1;
	FVector SelectedDestination = FVector::ZeroVector;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
