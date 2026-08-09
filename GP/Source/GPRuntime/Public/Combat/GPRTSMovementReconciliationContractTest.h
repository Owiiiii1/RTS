// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPRTSMovementReconciliationContractTest.generated.h"

/** GP-S33M RTS Movement Reconciliation contract runner. */
UCLASS()
class GPRUNTIME_API UGP_RTSMovementReconciliationContractTestRunner : public UObject
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
	void UnbindMovementResult();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle MovementResultHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> WalkerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> WalkerBWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> WalkerCWeak;
	TWeakObjectPtr<class AGP_Worker> EnemyWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerWeak;
	TWeakObjectPtr<class AGP_PlayerController> PCWeak;
	TWeakObjectPtr<class AGP_PlayerState> PSWeak;
	TWeakObjectPtr<AActor> WallWeak;
	TWeakObjectPtr<AActor> NavBoundsWeak;
	TWeakObjectPtr<class AGP_MainBase> BuildingWeak;
	TWeakObjectPtr<class UGP_MovementComponent> BoundMovementWeak;

	FVector Origin = FVector::ZeroVector;
	FVector PathDest = FVector::ZeroVector;
	FVector WallCenter = FVector::ZeroVector;
	FVector WallHalfExtent = FVector::ZeroVector;
	FVector GroupMovePoint = FVector::ZeroVector;
	FVector ClickDest = FVector::ZeroVector;
	FVector AttackMoveDest = FVector::ZeroVector;
	FVector UnreachableDest = FVector::ZeroVector;
	FVector SeparationMeet = FVector::ZeroVector;

	float SavedScanInterval = 0.35f;
	float SavedSightRange = 900.0f;
	float AcceptanceRadiusCm = 50.0f;
	bool bNavAvailable = false;
	bool bSerial1Cancelled = false;
	bool bSawMovementFailed = false;
	uint32 Serial1 = 0;
	uint32 Serial2 = 0;

	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
