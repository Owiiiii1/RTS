// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPUnitDefinitionContractTest.generated.h"

/** GP-S38D UnitDefinition initialization contract runner. */
UCLASS()
class GPRUNTIME_API UGP_UnitDefinitionContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_SalvageWalker> FallbackWalkerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> OverrideWalkerWeak;
	TWeakObjectPtr<class AGP_DefensiveTurret> TurretWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> AsyncWalkerWeak;
	TWeakObjectPtr<class AGP_SalvageWalker> FailWalkerWeak;
	int32 FailWaitTicks = 0;
	UPROPERTY()
	TObjectPtr<class UGP_UnitDefinition> OverrideDef;
	UPROPERTY()
	TObjectPtr<class UGP_UnitDefinition> AsyncDef;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
