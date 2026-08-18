// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPDefensiveTurretContractTest.generated.h"

/** GP-S37T Defensive Turret combat + orbital contract runner. */
UCLASS()
class GPRUNTIME_API UGP_DefensiveTurretContractTestRunner : public UObject
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
	void RestoreSettings();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_DefensiveTurret> TurretWeak;
	TWeakObjectPtr<class AGP_Worker> FriendlyWeak;
	TWeakObjectPtr<class AGP_Worker> FarEnemyWeak;
	TWeakObjectPtr<class AGP_Worker> NearEnemyWeak;
	TWeakObjectPtr<class AGP_Worker> ReacquireEnemyWeak;
	TWeakObjectPtr<class AGP_BuildingBase> FriendlyBuildingWeak;
	TWeakObjectPtr<class AGP_BuildingBase> DeadEnemyBuildingWeak;
	TWeakObjectPtr<class AGP_BuildingBase> EnemyBuildingWeak;
	TWeakObjectPtr<class AGP_BuildingBase> ReacquireBuildingWeak;
	TWeakObjectPtr<class AActor> BlockerWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_DropPod> LastPodWeak;
	TWeakObjectPtr<class AGP_DefensiveTurret> OrbitalTurretWeak;
	TArray<FIntPoint> OccupiedCells;
	FGuid OccupantId;
	float NearHpAfterFirstHit = 0.0f;
	float NearHpAfterCooldownWindow = 0.0f;
	float ReacquireHpAtBlock = 0.0f;
	float EnemyBuildingHpAtAcquire = 0.0f;
	float ReacquireBuildingHpAtAcquire = 0.0f;
	float OrbitalBeforePurchase = 0.0f;
	float OrbitalBeforeDeploy = 0.0f;
	int32 ReadyAfterPurchase = 0;
	FVector ValidDeployLocation = FVector::ZeroVector;
	FVector RejectDeployLocation = FVector::ZeroVector;
	float SavedBuildingDescent = 2.5f;
	float SavedBuildingCleanup = 0.5f;
	float SavedBuildingAltitude = 2500.0f;
	float SavedBuildingDeployDelay = 2.0f;
	float SavedBuildingMaxRadius = 5000.0f;
	bool bSettingsMutated = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
