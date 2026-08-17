// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPDropPod.h"
#include "UObject/Object.h"
#include "Units/GPWorker.h"
#include "GPUnitCapLogisticsHubContractTest.generated.h"

/** GP-S33C unit cap + Logistics Hub contract runner. */
UCLASS()
class GPRUNTIME_API UGP_UnitCapLogisticsHubContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_PlayerState> OtherPSWeak;
	TWeakObjectPtr<class AGP_DropPod> LastPodWeak;
	TWeakObjectPtr<class AGP_LogisticsHub> HubAWeak;
	TWeakObjectPtr<class AGP_LogisticsHub> HubBWeak;
	float SavedDescent = 2.5f;
	float SavedCleanup = 0.35f;
	float SavedAltitude = 2500.0f;
	float SavedDeployDelay = 1.25f;
	float SavedBuildingDescent = 2.5f;
	float SavedBuildingDeployDelay = 2.0f;
	float SavedBuildingCleanup = 0.5f;
	bool bSettingsMutated = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
