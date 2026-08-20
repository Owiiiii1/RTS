// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPFoWRuntimeFoundationContractTest.generated.h"

class AGP_MainBase;
class AGP_PlayerState;
class AGP_SalvageWalker;
class AGP_UnitBase;
class AGP_Worker;
class UGP_UnitDefinition;

/** Three-state per-team authoritative Fog of War runtime foundation contract runner. */
UCLASS()
class GPRUNTIME_API UGP_FoWRuntimeFoundationContractTestRunner : public UObject
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
	bool bCancelled = false;
	FName CancelReason;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;

	TWeakObjectPtr<AGP_Worker> PrimarySourceWeak;
	TWeakObjectPtr<AGP_Worker> UnionSourceWeak;
	TWeakObjectPtr<AGP_Worker> NonVisionSourceWeak;
	TWeakObjectPtr<AGP_Worker> DeadSourceWeak;
	TWeakObjectPtr<AGP_SalvageWalker> DamageSourceWeak;
	TWeakObjectPtr<AGP_SalvageWalker> AutoAcquireOwnerWeak;
	TWeakObjectPtr<AGP_Worker> AutoAcquireTargetWeak;
	TWeakObjectPtr<AGP_Worker> AutoAcquireRevealSourceWeak;
	TWeakObjectPtr<AGP_MainBase> PlacementMainBaseWeak;
	TWeakObjectPtr<AGP_PlayerState> PlacementPlayerStateWeak;

	UPROPERTY()
	TArray<TObjectPtr<UGP_UnitDefinition>> TestDefinitions;

	FVector PrimaryOriginalLocation = FVector::ZeroVector;
	FVector PrimaryMovedLocation = FVector::ZeroVector;
	FVector AutoAcquireLocation = FVector::ZeroVector;
	FVector PlacementVisibleLocation = FVector::ZeroVector;
	FVector PlacementHiddenLocation = FVector::ZeroVector;
};
