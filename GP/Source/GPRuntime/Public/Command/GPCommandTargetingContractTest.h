// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPCommandTargetingContractTest.generated.h"

/** MOVE / PATROL targeting + command cursor contract runner. */
UCLASS()
class GPRUNTIME_API UGP_CommandTargetingContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_Worker> WorkerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> WalkerWeak;
	TWeakObjectPtr<class AGP_PlayerController> LocalPCWeak;
	FVector Origin = FVector::ZeroVector;
	FVector MoveDest = FVector::ZeroVector;
	FVector PatrolDest = FVector::ZeroVector;
	FVector PatrolAnchorA = FVector::ZeroVector;
	int32 PatrolWaitTicks = 0;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
