// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPMineReassignmentHaulContractTest.generated.h"

/**
 * GP-S33M natural-chain contract:
 * ONE Mine(A) with A full → automatic reassignment to B → natural move/mine →
 * CargoFull → automatic haul → unload → return to B.
 * No teleport / second IssueMine / direct BeginMining on the tested worker after Mine(A).
 * Optional concurrent two-worker scenario included.
 */
UCLASS()
class GPRUNTIME_API UGP_MineReassignmentHaulContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_Worker> Worker2Weak;
	TWeakObjectPtr<class AGP_ResourceNode> NodeAWeak;
	TWeakObjectPtr<class AGP_ResourceNode> NodeBWeak;
	TArray<TWeakObjectPtr<class AGP_Worker>> FillerWorkers;
	FVector MainBaseLocation = FVector::ZeroVector;
	FVector NodeALocation = FVector::ZeroVector;
	FVector NodeBLocation = FVector::ZeroVector;
	int32 ContractTeamId = 1;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = 0.0;
	float MovementWaitTimeoutSeconds = 60.0f;
	float SavedSettingsSearchRadiusCm = 3000.0f;
	float SavedSettingsMaxPathLengthCm = 6000.0f;
	bool bSettingsOverridden = false;
	uint32 InitialMineSerial = 0;
	uint32 Worker2MineSerial = 0;
	bool bOwnershipProven = false;
	bool bCargoFullObserved = false;
	bool bIssueMineAfterInitial = false;
	bool bTeleportedAfterInitial = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
