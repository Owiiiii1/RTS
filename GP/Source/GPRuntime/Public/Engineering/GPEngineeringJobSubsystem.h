// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engineering/GPEngineeringJobTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GPEngineeringJobSubsystem.generated.h"

/**
 * World-scoped owner for local engineering jobs.
 * A planned job is not a Worker and is not GP.Command.Build.
 * This slice stores Terrain Leveling plans only. It does not deform terrain,
 * assign Workers, or replicate a client mirror.
 *
 * LevelHeightToleranceCm is a tunable default, not a final balance value.
 * MaxZoneAxisCells rejects accidental giant plans. It is not a design maximum for the game.
 */
UCLASS()
class GPRUNTIME_API UGP_EngineeringJobSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** A sample farther than this from TargetPlaneZ makes the cell NeedsLeveling. */
	static constexpr float LevelHeightToleranceCm = 15.f;

	/** Per-axis cap. 32 x 32 cells is the largest plan this slice will evaluate. */
	static constexpr int32 MaxZoneAxisCells = 32;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Authority only. TargetPlaneZ is stored as given.
	 * All-Level zones are rejected with NothingToLevel and are not stored.
	 * Any cell with no voxel surface rejects the plan with NoTerrain.
	 */
	FGP_TerrainLevelingCreateResult CreateTerrainLevelingJob(
		FIntPoint OriginCell,
		FIntPoint SizeCells,
		float TargetPlaneZ);

	const FGP_TerrainLevelingJob* FindTerrainLevelingJob(const FGuid& JobId) const;

	void GetTerrainLevelingJobs(TArray<FGP_TerrainLevelingJob>& OutJobs) const;

	/** Authority only. Removes a planned job. Does not deform terrain. */
	bool CancelTerrainLevelingJob(const FGuid& JobId);

	int32 GetTerrainLevelingJobCount() const { return Jobs.Num(); }

private:
	bool HasMatchAuthority() const;
	bool IsValidZoneSize(FIntPoint SizeCells) const;

	bool SampleVoxelSurfaceZ(const FVector& WorldXY, float ReferenceZ, float& OutSurfaceZ) const;

	void EvaluateCell(
		class UGP_BuildGridSubsystem* Grid,
		FIntPoint Cell,
		float TargetPlaneZ,
		FGP_TerrainLevelingCell& OutCell) const;

	TMap<FGuid, FGP_TerrainLevelingJob> Jobs;
};
