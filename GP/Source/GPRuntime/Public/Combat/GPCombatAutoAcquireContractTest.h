// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPCombatAutoAcquireContractTest.generated.h"

/** GP-S30R combat auto-acquire contract runner. */
UCLASS()
class GPRUNTIME_API UGP_CombatAutoAcquireContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_SalvageWalker> WalkerWeak;
	TWeakObjectPtr<class AGP_Worker> NearEnemyWeak;
	TWeakObjectPtr<class AGP_Worker> FarEnemyWeak;
	TWeakObjectPtr<class AGP_Worker> FriendlyWeak;
	float SavedScanInterval = 0.35f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
