// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "UObject/Object.h"
#include "GPBuildGridContractTest.generated.h"

UCLASS()
class GPRUNTIME_API AGP_BuildGridContractStub : public AGP_BuildingBase
{
	GENERATED_BODY()
};

UCLASS()
class GPRUNTIME_API UGP_BuildGridContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_DropPod> SkipPodWeak;
	TWeakObjectPtr<class AGP_LogisticsHub> LiveHubWeak;
	TWeakObjectPtr<class UGP_OrbitalDropDefinition> InvalidFootprintDropWeak;
	FVector ValidDeployLocation = FVector::ZeroVector;
	FVector AdjacentDeployLocation = FVector::ZeroVector;
	FVector SnappedExpected = FVector::ZeroVector;
	FIntPoint FirstHubOrigin = FIntPoint::ZeroValue;
	int32 MaxUnitsBeforeHub = 0;
	int32 ReadyBeforeInvalid = 0;
	float OrbitalBeforeInvalid = 0.0f;
	float SavedBuildingDeployDelay = 2.0f;
	float SavedBuildingDescent = 2.5f;
	float SavedBuildingCleanup = 0.5f;
	float SavedBuildingAltitude = 2500.0f;
	TSoftClassPtr<AGP_BuildingBase> SavedBuildingPayload;
	bool bSettingsMutated = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
