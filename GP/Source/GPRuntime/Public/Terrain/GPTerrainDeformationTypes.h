// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPTerrainDeformationTypes.generated.h"

/**
 * Geometry profile carried on a deformation event.
 * Only ShallowSphereCap has an implementation. Future irregular profiles must be
 * deterministic from Profile + Seed + RotationDegrees + RadiusCm + DepthCm.
 * Do not add random noise in the terrain service.
 */
UENUM(BlueprintType)
enum class EGP_TerrainDeformationProfile : uint8
{
	ShallowSphereCap UMETA(DisplayName = "Shallow Sphere Cap")
};

/**
 * Presentation hint only. The terrain service does not paint materials, spawn debris,
 * or edit vegetation. None is the only value until a scar consumer exists.
 */
UENUM(BlueprintType)
enum class EGP_TerrainSurfaceScarType : uint8
{
	None UMETA(DisplayName = "None")
};

UENUM()
enum class EGP_TerrainDeformationRejectReason : uint8
{
	None,
	NoAuthority,
	NonFinite,
	InvalidRadius,
	InvalidDepth,
	UnsupportedProfile,
	NoVoxelWorld,
	ApplyFailed
};

/** Public edit bounds. Voxel plugin types stay out of gameplay headers. */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_TerrainEditBounds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FIntVector Min = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FIntVector Max = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	bool bValid = false;
};

/**
 * Gameplay request. WorldLocation is the surface impact, not the sphere center.
 * The service derives the ShallowSphereCap center as
 * WorldLocation.Z + (RadiusCm - DepthCm).
 */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_TerrainDeformationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float RadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float DepthCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	EGP_TerrainDeformationProfile Profile = EGP_TerrainDeformationProfile::ShallowSphereCap;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float RotationDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	EGP_TerrainSurfaceScarType SurfaceScar = EGP_TerrainSurfaceScarType::None;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FName SourceIdentity = NAME_None;
};

/**
 * Compact authoritative record. Replicated as data only.
 * Voxel density payloads are not part of this struct.
 * WorldLocation remains the surface impact used to rebuild the profile.
 */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_TerrainDeformationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	int32 SequenceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float RadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float DepthCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	EGP_TerrainDeformationProfile Profile = EGP_TerrainDeformationProfile::ShallowSphereCap;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float RotationDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	EGP_TerrainSurfaceScarType SurfaceScar = EGP_TerrainSurfaceScarType::None;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FName SourceIdentity = NAME_None;
};

USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_TerrainDeformationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	int32 SequenceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	EGP_TerrainDeformationRejectReason Reason = EGP_TerrainDeformationRejectReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float AppliedRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	float AppliedDepthCm = 0.f;

	/** Sphere center used for ShallowSphereCap. Zero when rejected. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FVector EditCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	FGP_TerrainEditBounds EditedBounds;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Terrain")
	TObjectPtr<AActor> ResolvedVoxelWorld = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGP_TerrainDeformationApplied, const FGP_TerrainDeformationEvent& /*Event*/);
