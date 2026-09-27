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
 * Shape is SphereSubtract. There is no Depth parameter on RemoveSphere.
 * A shallow crater places the sphere center ABOVE the surface by (RadiusCm - DepthCm).
 * Depth is the lower cap's penetration under the surface, in world centimeters.
 */
namespace GPVoxelCraterMath
{
	constexpr float DefaultRadiusCm = 400.f;
	constexpr float DefaultDepthCm = 80.f;
	constexpr float MinRadiusCm = 50.f;
	constexpr float MaxRadiusCm = 1500.f;
	constexpr float MinDepthCm = 10.f;

	struct FShallowCrater
	{
		FVector EditCenter = FVector::ZeroVector;
		float RadiusCm = DefaultRadiusCm;
		float DepthCm = DefaultDepthCm;
		float CenterZOffsetCm = DefaultRadiusCm - DefaultDepthCm;
	};

	inline FShallowCrater Make(const FVector& ImpactPoint, float RequestedRadiusCm, float RequestedDepthCm)
	{
		FShallowCrater Out;
		const float RadiusSource = FMath::IsFinite(RequestedRadiusCm) ? RequestedRadiusCm : DefaultRadiusCm;
		Out.RadiusCm = FMath::Clamp(RadiusSource, MinRadiusCm, MaxRadiusCm);
		const float DepthSource = FMath::IsFinite(RequestedDepthCm) ? RequestedDepthCm : DefaultDepthCm;
		const float DepthMax = FMath::Max(MinDepthCm, Out.RadiusCm);
		Out.DepthCm = FMath::Clamp(DepthSource, MinDepthCm, DepthMax);
		Out.CenterZOffsetCm = Out.RadiusCm - Out.DepthCm;
		Out.EditCenter = FVector(ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z + Out.CenterZOffsetCm);
		return Out;
	}
}

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

enum class EGPVoxelWorldResolvePath : uint8
{
	None,
	HitActor,
	ComponentOuter,
	AttachParent,
	ActorOuter
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

	bool ApplySphereAdd(
		AActor* VoxelWorldActor,
		const FGPVoxelSphereSubtractRequest& Request,
		FGPVoxelIntBoxReport& OutEditedBounds);

	AActor* ResolveVoxelWorldFromHit(const FHitResult& Hit, EGPVoxelWorldResolvePath& OutPath);

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
