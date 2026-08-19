// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPLogisticsHub.h"
#include "Orbital/GPDropPod.h"
#include "UObject/Object.h"
#include "GPOrbitalBuildingDropContractTest.generated.h"

/** Contract-only Logistics Hub subclass (authored BP stand-in). */
UCLASS()
class GPRUNTIME_API AGP_OrbitalBuildingDropContractHubStub : public AGP_LogisticsHub
{
	GENERATED_BODY()
};

/** Contract-only Defensive Turret subclass (authored BP stand-in). */
UCLASS()
class GPRUNTIME_API AGP_OrbitalBuildingDropContractTurretStub : public AGP_DefensiveTurret
{
	GENERATED_BODY()
};

/** GP-S32R orbital building drop contract runner. */
UCLASS()
class GPRUNTIME_API UGP_OrbitalBuildingDropContractTestRunner : public UObject
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
	void CleanupCatalogIfExists();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_DropPod> LastPodWeak;
	float OrbitalBeforePurchase = 0.0f;
	float OrbitalBeforeDeploy = 0.0f;
	FVector ValidDeployLocation = FVector::ZeroVector;
	float SavedBuildingCleanup = 0.5f;
	float SavedBuildingAltitude = 2500.0f;
	float SavedBuildingMaxRadius = 5000.0f;
	bool bSettingsMutated = false;
	UPROPERTY()
	TObjectPtr<class UGP_OrbitalDropDefinition> AuthoredHubDropDef;
	UPROPERTY()
	TObjectPtr<class UGP_OrbitalDropDefinition> AuthoredTurretDropDef;
	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> AuthoredHubBuildingDef;
	UPROPERTY()
	TObjectPtr<class UGP_BuildingDefinition> AuthoredTurretBuildingDef;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
