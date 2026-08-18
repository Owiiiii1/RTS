// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPCombatRetaliationPursuitContractTest.generated.h"

/** GP-S40R timed retaliation pursuit contract runner. */
UCLASS()
class GPRUNTIME_API UGP_CombatRetaliationPursuitContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_SalvageWalker> VictimWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> AttackerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> AttackerBWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> EngageVictimWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> EngageAttackerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> ManualVictimWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerVictimWeak;
	TWeakObjectPtr<class AGP_DefensiveTurret> TurretWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> LOSVictimWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> LOSAttackerWeak;
	TWeakObjectPtr<class AActor> LOSBlockerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> TimeoutLOSVictimWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> TimeoutLOSAttackerWeak;
	TWeakObjectPtr<class AActor> TimeoutLOSBlockerWeak;
	TWeakObjectPtr<class UGP_UnitDefinition> ShortRetaliationDefWeak;
	FVector Origin = FVector::ZeroVector;
	uint32 ManualMoveSerial = 0;
	uint32 AttackHandoffSerial = 0;
	float RemainingAfterFirstHit = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
