// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPEngineeringJobTypes.generated.h"

/** Only TerrainLeveling is implemented. Other local-engineering types are not jobs yet. */
UENUM()
enum class EGP_EngineeringJobType : uint8
{
	TerrainLeveling
};

/** Planned exists with zero Workers. Completed is reserved until work can finish a job. */
UENUM()
enum class EGP_EngineeringJobState : uint8
{
	Planned,
	Completed
};

/**
 * Factual cell classification. Grey / yellow are presentation names for Level / NeedsLeveling.
 * NoTerrain means the cell had no voxel surface and a job containing it is not stored.
 */
UENUM()
enum class EGP_TerrainLevelingCellState : uint8
{
	Level,
	NeedsLeveling,
	NoTerrain
};

UENUM()
enum class EGP_EngineeringJobRejectReason : uint8
{
	None,
	NoAuthority,
	InvalidSize,
	NonFiniteTarget,
	NoBuildGrid,
	NoTerrain,
	NothingToLevel
};

USTRUCT()
struct GPRUNTIME_API FGP_TerrainLevelingCell
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY()
	EGP_TerrainLevelingCellState State = EGP_TerrainLevelingCellState::NoTerrain;

	/** Mean Z of the voxel samples that hit. Zero when the cell has no terrain. */
	UPROPERTY()
	float MeanSurfaceZ = 0.f;

	UPROPERTY()
	float MinSurfaceZ = 0.f;

	UPROPERTY()
	float MaxSurfaceZ = 0.f;

	UPROPERTY()
	float MaxAbsDeviationCm = 0.f;

	UPROPERTY()
	int32 SampleCount = 0;
};

/**
 * Domain record for one planned leveling zone.
 * Worker assignment, progress, work positions, and presentation pulses are not stored yet.
 * This struct is the shape a later client mirror can copy. It is not replicated in this slice.
 */
USTRUCT()
struct GPRUNTIME_API FGP_TerrainLevelingJob
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid JobId;

	UPROPERTY()
	EGP_EngineeringJobType Type = EGP_EngineeringJobType::TerrainLeveling;

	UPROPERTY()
	EGP_EngineeringJobState State = EGP_EngineeringJobState::Planned;

	UPROPERTY()
	FIntPoint OriginCell = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint SizeCells = FIntPoint::ZeroValue;

	/** Explicit construction plane. The job does not derive this from the terrain. */
	UPROPERTY()
	float TargetPlaneZ = 0.f;

	UPROPERTY()
	TArray<FGP_TerrainLevelingCell> Cells;

	UPROPERTY()
	int32 LevelCellCount = 0;

	UPROPERTY()
	int32 NeedsLevelingCellCount = 0;
};

USTRUCT()
struct GPRUNTIME_API FGP_TerrainLevelingCreateResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAccepted = false;

	UPROPERTY()
	FGuid JobId;

	UPROPERTY()
	EGP_EngineeringJobRejectReason Reason = EGP_EngineeringJobRejectReason::None;

	UPROPERTY()
	FIntPoint OriginCell = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint SizeCells = FIntPoint::ZeroValue;

	UPROPERTY()
	float TargetPlaneZ = 0.f;

	UPROPERTY()
	int32 TotalCells = 0;

	UPROPERTY()
	int32 LevelCells = 0;

	UPROPERTY()
	int32 NeedsLevelingCells = 0;

	/** Filled whenever the zone was evaluated, including NothingToLevel. Empty on early reject. */
	UPROPERTY()
	TArray<FGP_TerrainLevelingCell> Cells;
};
