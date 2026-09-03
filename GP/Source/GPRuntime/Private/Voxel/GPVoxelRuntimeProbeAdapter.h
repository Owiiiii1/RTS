// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FHitResult;

/**
 * Private GPRuntime Voxel Plugin adapter for Stage 3A runtime crater probes.
 * Voxel types stay in the .cpp. Public gameplay headers must not include this.
 *
 * Experimental request consumed by RemoveSphere: WorldLocation (cm) + RadiusCm.
 * Shape is SphereSubtract. There is no Depth parameter on RemoveSphere; a deeper
 * crater is a sphere whose center is offset below the surface, not a separate API.
 */
struct FGPVoxelSphereSubtractRequest
{
	FVector WorldLocation = FVector::ZeroVector;
	float RadiusCm = 300.f;
};

struct FGPVoxelIntBoxReport
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	bool bValid = false;
	bool bInfinite = false;
};

namespace GPVoxelRuntimeProbeAdapter
{
	static const FName ProbeActorTag(TEXT("GP_VoxelRuntimeProbe"));

	AActor* FindExistingProbe(UWorld* World);
	AActor* SpawnConfiguredProbeWorld(UWorld* World, const FVector& ActorLocation);
	bool CreateWorldIfNeeded(AActor* VoxelWorldActor);
	bool IsCreated(const AActor* VoxelWorldActor);
	bool IsLoaded(const AActor* VoxelWorldActor);
	bool IsMeshIdle(const AActor* VoxelWorldActor);
	int32 GetMeshTaskCount(const AActor* VoxelWorldActor);
	int32 CountProcMeshComponents(const AActor* VoxelWorldActor);

	bool ApplySphereSubtract(
		AActor* VoxelWorldActor,
		const FGPVoxelSphereSubtractRequest& Request,
		FGPVoxelIntBoxReport& OutEditedBounds);

	bool QueryDensityAtVoxel(AActor* VoxelWorldActor, const FIntVector& VoxelCoord, float& OutDensity);
	bool QueryDensityAtWorld(AActor* VoxelWorldActor, const FVector& WorldLocation, float& OutDensity, FIntVector& OutVoxelCoord);

	FVector VoxelToWorld(AActor* VoxelWorldActor, const FIntVector& VoxelCoord);
	FIntVector WorldToVoxel(AActor* VoxelWorldActor, const FVector& WorldLocation);
	float GetVoxelSizeCm(AActor* VoxelWorldActor);
	FIntVector GetWorldBoundsSize(AActor* VoxelWorldActor);

	bool LineTraceHitsProbe(
		UWorld* World,
		AActor* VoxelWorldActor,
		const FVector& Start,
		const FVector& End,
		FHitResult& OutHit);

	bool QueryClosestNonEmpty(
		AActor* VoxelWorldActor,
		const FVector& WorldLocation,
		bool& bSuccess,
		FIntVector& OutVoxel,
		float& OutDensity);

	bool QueryDensitySurfaceWorldZ(
		AActor* VoxelWorldActor,
		float WorldX,
		float WorldY,
		float& OutWorldZ);

	void DestroyProbeWorld(AActor* VoxelWorldActor);
}
