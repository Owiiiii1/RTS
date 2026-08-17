// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "UObject/Object.h"
#include "GPMultiBuildingDataContractTest.generated.h"

UCLASS()
class GPRUNTIME_API AGP_MultiBuildingDataContractStubA : public AGP_BuildingBase
{
	GENERATED_BODY()
};

UCLASS()
class GPRUNTIME_API AGP_MultiBuildingDataContractStubB : public AGP_BuildingBase
{
	GENERATED_BODY()
};

UCLASS()
class GPRUNTIME_API UGP_MultiBuildingDataContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_LogisticsHub> LiveHubWeak;
	TWeakObjectPtr<class UGP_OrbitalDropDefinition> DropAWeak;
	TWeakObjectPtr<class UGP_OrbitalDropDefinition> DropBWeak;
	TWeakObjectPtr<class UGP_OrbitalDropDefinition> DropCWeak;
	float OrbitalBeforePurchaseA = 0.0f;
	float OrbitalBeforeDeployA = 0.0f;
	int32 MaxUnitsBeforeHub = 0;
	FVector ValidDeployLocation = FVector::ZeroVector;
	float SavedBuildingDeployDelay = 2.0f;
	float SavedBuildingDescent = 2.5f;
	float SavedBuildingCleanup = 0.5f;
	float SavedBuildingAltitude = 2500.0f;
	bool bSettingsMutated = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
