// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnit.h"
#include "GPSalvageWalker.generated.h"

/**
 * Canonical MVP combat unit (GDD/04 Salvage Walker).
 * Concrete AGP_Unit child: existing Attack FSM / LOS / GAS / presentation stack.
 * Operator BP_SalvageWalker subclasses this class with AuthoredComponents visuals.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_SalvageWalker : public AGP_Unit
{
	GENERATED_BODY()

public:
	AGP_SalvageWalker();
};

/** Composition / GDD defaults contract for native AGP_SalvageWalker (no Blueprint asset). */
UCLASS()
class GPRUNTIME_API UGP_SalvageWalkerContractTestRunner : public UObject
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

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_SalvageWalker> UnitWeak;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
