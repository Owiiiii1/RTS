// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPCombatAttackMoveContractTest.generated.h"

/** GP-S32A Attack-Move contract runner. */
UCLASS()
class GPRUNTIME_API UGP_CombatAttackMoveContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_Worker> EnemyWeak;
	TWeakObjectPtr<class AGP_Worker> EnemyAltWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerWeak;
	TWeakObjectPtr<class AGP_BuildingBase> BuildingTargetWeak;
	TWeakObjectPtr<class AGP_PlayerController> PCWeak;
	TWeakObjectPtr<class AGP_PlayerState> PSWeak;
	FVector Origin = FVector::ZeroVector;
	FVector AttackMoveDestA = FVector::ZeroVector;
	FVector AttackMoveDestB = FVector::ZeroVector;
	FVector AttackMoveDestH1 = FVector::ZeroVector;
	FVector AttackMoveDestH2 = FVector::ZeroVector;
	FVector ExplicitMoveDest = FVector::ZeroVector;
	float SavedScanInterval = 0.35f;
	float SavedSightRange = 900.0f;
	float EnemyHpAtAcquire = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
