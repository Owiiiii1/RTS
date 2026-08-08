// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Orbital/GPUnitDropManifest.h"
#include "GPDropPod.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AGP_PlayerState;

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

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	float GetDescentProgress01() const { return DescentProgress01; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	bool IsDescending() const { return Phase == EGP_DropPodPhase::Descending; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	EGP_DropPodPhase GetPhase() const { return Phase; }

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
	void AuthorityScheduleCleanup();
	void HandleCleanup();
	void ClearLifecycleTimers();
	void ApplyNativePlaceholderVisibility();
	void HideNativePlaceholder();
	void AuthoritySetPhase(EGP_DropPodPhase NewPhase);

	FGP_UnitDropManifest PendingManifest;
	TWeakObjectPtr<AGP_PlayerState> RequestingPlayerStateWeak;
	float DescentDurationSeconds = 2.5f;
	float SpawnSpacingCm = 180.0f;
	float PayloadDeployDelaySeconds = 1.25f;
	float CleanupDelaySeconds = 0.35f;
	float DescentElapsed = 0.0f;
	bool bLandingCompleted = false;
	bool bPayloadSpawned = false;
	FTimerHandle DeployTimerHandle;
	FTimerHandle CleanupTimerHandle;
};
