// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPVoxelRuntimeCraterProbeContractTest.generated.h"

class AActor;
class UWorld;

UCLASS()
class GPRUNTIME_API UGP_VoxelRuntimeCraterProbeContractTestRunner : public UObject
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
	void ScheduleNext(float DelaySeconds);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();

	int32 StageIndex = 0;
	int32 Failures = 0;
	int32 ReadyWaitTicks = 0;
	int32 MeshIdleWaitTicks = 0;
	int32 CollisionWaitTicks = 0;
	bool bFinished = false;
	bool bCancelled = false;
	FName CancelReason = NAME_None;
	uint64 ExecutionId = 0;
	FName OwnerTag = NAME_None;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AActor> ProbeWeak;

	FVector ProbeOrigin = FVector::ZeroVector;
	FVector CraterWorld = FVector::ZeroVector;
	FVector FarWorld = FVector::ZeroVector;
	FIntVector CraterVoxel = FIntVector::ZeroValue;
	FIntVector FarVoxel = FIntVector::ZeroValue;
	float BaselineCraterDensity = 0.f;
	float BaselineFarDensity = 0.f;
	float BaselineCraterHitZ = 0.f;
	float BaselineFarHitZ = 0.f;
	float AfterCraterDensity = 0.f;
	float AfterFarDensity = 0.f;
	int32 BaselineMeshCount = 0;
	int32 AfterMeshCount = 0;
};
