// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPDefensiveTurret.h"
#include "UObject/Object.h"
#include "GPBuildingVitalsOwnershipContractTest.generated.h"

/** Contract-only building with deliberately configurable actor fallback values. */
UCLASS()
class GPRUNTIME_API AGP_BuildingVitalsOwnershipContractStub : public AGP_DefensiveTurret
{
	GENERATED_BODY()

public:
	void ConfigureFallbacks(
		float MaxHealth,
		float Health,
		float Damage,
		float Armor,
		float Resistance,
		float Cooldown,
		float Range);
};

/** BuildingDefinition -> UnitDefinition -> UnitBase/GAS ownership contract. */
UCLASS()
class GPRUNTIME_API UGP_BuildingVitalsOwnershipContractTestRunner : public UObject
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
	void CleanupActors();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();

	int32 StageIndex = 0;
	int32 Failures = 0;
	int32 FailureWaitTicks = 0;
	bool bFinished = false;
	bool bCancelled = false;
	FName CancelReason;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TArray<TWeakObjectPtr<AGP_BuildingVitalsOwnershipContractStub>> Buildings;

	UPROPERTY()
	TObjectPtr<class UGP_UnitDefinition> CanonicalDefinition;

	UPROPERTY()
	TObjectPtr<class UGP_UnitDefinition> ConflictingActorDefinition;

	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> CanonicalBuildingDefinition;

	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> FailingBuildingDefinition;

	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> EmptyBuildingDefinition;
};
