// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Orbital/GPUnitDropManifest.h"
#include "GPDropPod.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AGP_PlayerState;
class AGP_BuildingBase;
class AGP_MainBase;

UENUM(BlueprintType)
enum class EGP_DropPodPayloadKind : uint8
{
	Unit = 0,
	Building,
	WallPackage
};

UENUM(BlueprintType)
enum class EGP_DropPodPhase : uint8
{
	Idle = 0,
	Descending,
	Deploying,
	PayloadDeployed
};

/**
 * Shared orbital DropPod (GP-S31R unit manifests; later building payloads).
 * Lifecycle: Descending → Impact/Deploying → PayloadDeployed → Cleanup.
 * Presentation via replicated phase + NetMulticast → BlueprintImplementableEvents.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_DropPod : public AActor
{
	GENERATED_BODY()

public:
	AGP_DropPod();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Authority-only. Schedules descent → deploy delay → payload → cleanup. */
	void AuthorityInitUnitDrop(
		AGP_PlayerState* RequestingPlayerState,
		int32 TeamId,
		const FVector& LandingWorldLocation,
		const FRotator& LandingWorldRotation,
		const FGP_UnitDropManifest& Manifest,
		float DescentDurationSeconds,
		float SpawnAltitudeCm,
		float SpawnSpacingCm,
		float PayloadDeployDelaySeconds,
		float CleanupDelaySeconds);

	/** Authority-only. Schedules descent → deploy delay → building payload → cleanup. */
	void AuthorityInitBuildingDrop(
		AGP_PlayerState* RequestingPlayerState,
		int32 TeamId,
		FPrimaryAssetId DropDefinitionId,
		TSubclassOf<AGP_BuildingBase> PayloadClass,
		const FVector& LandingWorldLocation,
		const FRotator& LandingWorldRotation,
		float DescentDurationSeconds,
		float SpawnAltitudeCm,
		float PayloadDeployDelaySeconds,
		float CleanupDelaySeconds,
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		FGuid GridReservationId);

	/** Authority-only. Package inventory arrival at MainBase. No AGP_Wall spawn. No grid reservation. */
	void AuthorityInitWallPackageDrop(
		AGP_PlayerState* RequestingPlayerState,
		int32 TeamId,
		AGP_MainBase* TargetMainBase,
		int32 DeliveryGeneration,
		int32 SegmentCount,
		const FVector& LandingWorldLocation,
		const FRotator& LandingWorldRotation,
		float DescentDurationSeconds,
		float SpawnAltitudeCm,
		float PayloadDeployDelaySeconds,
		float CleanupDelaySeconds);

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	EGP_DropPodPayloadKind GetPayloadKind() const { return PayloadKind; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	FVector GetLandingLocation() const { return LandingLocation; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	float GetDescentProgress01() const { return DescentProgress01; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	bool IsDescending() const { return Phase == EGP_DropPodPhase::Descending; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	EGP_DropPodPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetBuildingGridOriginCell() const { return BuildingGridOriginCell; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetBuildingGridFootprintSize() const { return BuildingGridFootprintSize; }

#if !UE_BUILD_SHIPPING
	/** Contract seam: accepted pod skips payload spawn and releases reservation. */
	void DebugForceSkipPayloadSpawn() { bDebugSkipPayloadSpawn = true; }
	float DebugGetDescentDurationSeconds() const { return DescentDurationSeconds; }
	float DebugGetPayloadDeployDelaySeconds() const { return PayloadDeployDelaySeconds; }
	TSubclassOf<AGP_BuildingBase> DebugGetPendingBuildingPayloadClass() const { return PendingBuildingPayloadClass; }
#endif

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnDescentStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnImpact();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnPayloadDeployed();

	/**
	 * When true, native Engine cylinder PlaceholderMesh is shown during descent.
	 * Hidden on Impact. Authored BP_DropPod_MVP should set false and supply own mesh/Niagara.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|DropPod|Presentation")
	bool bUseNativePlaceholder = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|DropPod")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Engine BasicShapes cylinder — placeholder until BP authors rocket mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|DropPod")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(Replicated)
	FVector LandingLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator LandingRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated)
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	int32 OwnerTeamId = 0;

	UPROPERTY(Replicated)
	float DescentProgress01 = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EGP_DropPodPhase Phase = EGP_DropPodPhase::Idle;

	UPROPERTY(Replicated)
	EGP_DropPodPayloadKind PayloadKind = EGP_DropPodPayloadKind::Unit;

	/** Tiny client-preview seam for in-flight building reservation (not full grid replication). */
	UPROPERTY(Replicated)
	FIntPoint BuildingGridOriginCell = FIntPoint::ZeroValue;

	UPROPERTY(Replicated)
	FIntPoint BuildingGridFootprintSize = FIntPoint::ZeroValue;

	UFUNCTION()
	void OnRep_Phase(EGP_DropPodPhase PreviousPhase);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PresentationDescentStarted();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PresentationImpact();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PresentationPayloadDeployed();

private:
	void AuthorityCompleteLanding();
	void AuthorityBeginPayloadDeploy();
	void AuthoritySpawnUnitPayload();
	void AuthoritySpawnBuildingPayload();
	void AuthorityDeliverWallPackage();
	void AuthorityCancelWallPackageIfPending();
	void AuthorityScheduleCleanup();
	void HandleCleanup();
	void ClearLifecycleTimers();
	void ApplyNativePlaceholderVisibility();
	void HideNativePlaceholder();
	void AuthoritySetPhase(EGP_DropPodPhase NewPhase);
	void AuthorityReleaseLeftoverUnitReservation();
	void AuthorityReleaseBuildingGridReservation();

	FGP_UnitDropManifest PendingManifest;
	FPrimaryAssetId PendingDropDefinitionId;
	TSubclassOf<AGP_BuildingBase> PendingBuildingPayloadClass;
	FGuid BuildingGridReservationId;
	bool bGridReservationPromoted = false;
	TWeakObjectPtr<AGP_PlayerState> RequestingPlayerStateWeak;
	TWeakObjectPtr<AGP_MainBase> WallPackageMainBaseWeak;
	int32 WallPackageDeliveryGeneration = 0;
	int32 WallPackageSegmentCount = 0;
	int32 WallPackageExpectedTeamId = 0;
	bool bWallPackageDelivered = false;
	float DescentDurationSeconds = 2.5f;
	float SpawnSpacingCm = 180.0f;
	float PayloadDeployDelaySeconds = 1.25f;
	float CleanupDelaySeconds = 0.35f;
	float DescentElapsed = 0.0f;
	bool bLandingCompleted = false;
	bool bPayloadSpawned = false;
	int32 RemainingUnitReservation = 0;
#if !UE_BUILD_SHIPPING
	bool bDebugSkipPayloadSpawn = false;
#endif
	FTimerHandle DeployTimerHandle;
	FTimerHandle CleanupTimerHandle;
};
