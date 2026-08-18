// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPWallPackageInventoryContractTest.generated.h"

UCLASS()
class GPRUNTIME_API UGP_WallPackageInventoryContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

	UFUNCTION()
	void HandleStockChanged(int32 NewCount);

	UFUNCTION()
	void HandlePendingChanged(bool bPending);

private:
	void ScheduleNext(float DelaySeconds = 0.05f);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();
	void BindInventoryDelegates();
	void UnbindInventoryDelegates();
	void CleanupCatalogIfExists();
	bool WaitForStock(class UGP_WallSegmentInventoryComponent* Inventory, int32 ExpectedStock, int32 RetryStage);

	int32 StageIndex = 0;
	int32 ArrivalWaitAttempts = 0;
	int32 ExpectedArrivalStock = 5;
	float OrbitalAtPending = 0.0f;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_DropPod> LastPodWeak;
	TObjectPtr<class UGP_WallPackageDefinition> AuthoredPackage;
	float OrbitalBefore = 0.0f;
	int32 StockBroadcasts = 0;
	int32 PendingBroadcasts = 0;
	int32 LastBroadcastStock = -1;
	bool bLastBroadcastPending = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
