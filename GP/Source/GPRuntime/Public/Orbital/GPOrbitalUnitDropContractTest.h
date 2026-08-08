// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPDropPod.h"
#include "UObject/Object.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPWorker.h"
#include "GPOrbitalUnitDropContractTest.generated.h"

/** Contract-only Worker subclass (authored BP stand-in). */
UCLASS()
class GPRUNTIME_API AGP_OrbitalDropContractWorkerStub : public AGP_Worker
{
	GENERATED_BODY()
};

/** Contract-only Salvage Walker subclass (authored BP stand-in). */
UCLASS()
class GPRUNTIME_API AGP_OrbitalDropContractWalkerStub : public AGP_SalvageWalker
{
	GENERATED_BODY()
};

/** Contract-only DropPod subclass (authored BP stand-in). */
UCLASS()
class GPRUNTIME_API AGP_OrbitalDropContractPodStub : public AGP_DropPod
{
	GENERATED_BODY()
};

/** GP-S31R orbital unit drop contract runner. */
UCLASS()
class GPRUNTIME_API UGP_OrbitalUnitDropContractTestRunner : public UObject
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
	void RestoreSettings();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_DropPod> LastPodWeak;
	float OrbitalBeforeSpend = 0.0f;
	float SavedDescent = 2.5f;
	float SavedCleanup = 0.35f;
	float SavedAltitude = 2500.0f;
	float SavedDeployDelay = 1.25f;
	TSoftClassPtr<AGP_Worker> SavedWorkerPayload;
	TSoftClassPtr<AGP_SalvageWalker> SavedWalkerPayload;
	TSoftClassPtr<AGP_DropPod> SavedDropPodClass;
	bool bSettingsMutated = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
