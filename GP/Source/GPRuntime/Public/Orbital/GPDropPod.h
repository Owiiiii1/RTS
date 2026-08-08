// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Orbital/GPUnitDropManifest.h"
#include "GPDropPod.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AGP_PlayerState;

/**
 * Shared orbital DropPod (GP-S31R unit manifests; later building payloads).
 * Native lifecycle + placeholder mesh. Authored BP may replace mesh/Niagara without C++ rewrite.
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

	/** Authority-only. Schedules descent then payload spawn. */
	void AuthorityInitUnitDrop(
		AGP_PlayerState* RequestingPlayerState,
		int32 TeamId,
		const FVector& LandingWorldLocation,
		const FRotator& LandingWorldRotation,
		const FGP_UnitDropManifest& Manifest,
		float DescentDurationSeconds,
		float SpawnAltitudeCm,
		float SpawnSpacingCm,
		float CleanupDelaySeconds);

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	float GetDescentProgress01() const { return DescentProgress01; }

	UFUNCTION(BlueprintPure, Category = "GP|DropPod")
	bool IsDescending() const { return bDescending; }

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnDescentStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnImpact();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|DropPod|Presentation")
	void OnPayloadDeployed();

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

	UPROPERTY(Replicated)
	bool bDescending = false;

private:
	void AuthorityCompleteLanding();
	void AuthoritySpawnUnitPayload();
	void AuthorityScheduleCleanup();
	void HandleCleanup();

	FGP_UnitDropManifest PendingManifest;
	TWeakObjectPtr<AGP_PlayerState> RequestingPlayerStateWeak;
	float DescentDurationSeconds = 2.5f;
	float SpawnSpacingCm = 180.0f;
	float CleanupDelaySeconds = 0.35f;
	float DescentElapsed = 0.0f;
	bool bLandingCompleted = false;
	FTimerHandle CleanupTimerHandle;
};
