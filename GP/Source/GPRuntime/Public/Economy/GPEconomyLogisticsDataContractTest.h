// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPEconomyLogisticsDataContractTest.generated.h"

/** GP-S39E economy / logistics data ownership contract runner. */
UCLASS()
class GPRUNTIME_API UGP_EconomyLogisticsDataContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_Worker> WorkerDefWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerFallbackWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseDefWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseFallbackWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseAsyncWeak;
	TWeakObjectPtr<class AGP_Worker> WorkerAsyncWeak;
	TWeakObjectPtr<class AGP_ResourceNode> ResourceNodeWeak;
	TWeakObjectPtr<class AGP_ResourceNode> ResourceNodeOverrideWeak;
	TWeakObjectPtr<class AGP_LogisticsHub> HubWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_MainBase> SpendBaseWeak;
	TWeakObjectPtr<class AGP_DropPod> SpendPodWeak;
	float OrbitalBeforeSpend = 0.0f;
	UPROPERTY()
	TObjectPtr<class UGP_UnitDefinition> CargoOverrideDef;
	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> StorageOverrideDef;
	UPROPERTY()
	TObjectPtr<class UGP_ResourceDefinition> FerroniteDef;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
