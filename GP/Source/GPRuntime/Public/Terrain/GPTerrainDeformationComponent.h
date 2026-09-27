// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Terrain/GPTerrainDeformationTypes.h"
#include "GPTerrainDeformationComponent.generated.h"

/**
 * Match-global terrain deformation owner. Default subobject of AGP_GameState.
 *
 * Authority creates events through RequestDeformation. Each machine applies the
 * same profile locally. Clients do not author events.
 *
 * MaxEventHistory (32) is an intermediate reconstruction buffer, not a late-join
 * solution and not a snapshot. A match with more than 32 accepted events does
 * not retain the older ones for a client that never applied them. Connected
 * clients that already applied a trimmed event are unaffected. Snapshot
 * compaction is future network work. Dense voxel data is never replicated.
 *
 * ShallowSphereCap rotation has no visible effect. Future irregular profiles must
 * be deterministic from Profile + Seed + RotationDegrees + geometry parameters.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_TerrainDeformationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_TerrainDeformationComponent();

	static constexpr int32 MaxEventHistory = 32;

	/** Authority only. Clients return NoAuthority and do not edit terrain. */
	FGP_TerrainDeformationResult RequestDeformation(const FGP_TerrainDeformationRequest& Request);

	/**
	 * Apply log entries whose SequenceId has not been applied on this machine.
	 * Idempotent. Listen-server authority apply plus this call does not double-edit.
	 * A failed local apply stays pending so a later call can retry.
	 */
	void ApplyPendingAuthoritativeEvents();

	const TArray<FGP_TerrainDeformationEvent>& GetEventLog() const { return EventLog; }
	bool HasAppliedSequence(int32 SequenceId) const { return AppliedSequenceIds.Contains(SequenceId); }
	int32 GetAppliedCount() const { return AppliedSequenceIds.Num(); }

	/** Fired after a local geometry apply. Presentation, scars, and vegetation subscribe later. */
	FOnGP_TerrainDeformationApplied OnTerrainDeformationApplied;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_EventLog();

private:
	bool NormalizeRequest(
		const FGP_TerrainDeformationRequest& Request,
		FGP_TerrainDeformationRequest& OutNormalized,
		EGP_TerrainDeformationRejectReason& OutReason) const;

	AActor* ResolveOwningVoxelWorld(const FVector& WorldLocation) const;

	bool ApplyEventGeometry(
		const FGP_TerrainDeformationEvent& Event,
		FGP_TerrainEditBounds& OutBounds,
		FVector& OutEditCenter,
		AActor*& OutVoxelWorld) const;

	UPROPERTY(ReplicatedUsing = OnRep_EventLog)
	TArray<FGP_TerrainDeformationEvent> EventLog;

	TSet<int32> AppliedSequenceIds;
	int32 NextSequenceId = 0;
};
