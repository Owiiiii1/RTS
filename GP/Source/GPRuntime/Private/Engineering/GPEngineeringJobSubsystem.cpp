// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineering/GPEngineeringJobSubsystem.h"

#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Units/GPUnitBase.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPEngineering, Log, All);

namespace GPEngineeringPrivate
{
	constexpr float TraceHalfExtentCm = 8000.f;

	static const TCHAR* ReasonName(EGP_EngineeringJobRejectReason Reason)
	{
		switch (Reason)
		{
		case EGP_EngineeringJobRejectReason::None:
			return TEXT("None");
		case EGP_EngineeringJobRejectReason::NoAuthority:
			return TEXT("NoAuthority");
		case EGP_EngineeringJobRejectReason::InvalidSize:
			return TEXT("InvalidSize");
		case EGP_EngineeringJobRejectReason::NonFiniteTarget:
			return TEXT("NonFiniteTarget");
		case EGP_EngineeringJobRejectReason::NoBuildGrid:
			return TEXT("NoBuildGrid");
		case EGP_EngineeringJobRejectReason::NoTerrain:
			return TEXT("NoTerrain");
		case EGP_EngineeringJobRejectReason::NothingToLevel:
			return TEXT("NothingToLevel");
		default:
			return TEXT("Unknown");
		}
	}
}

bool UGP_EngineeringJobSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

bool UGP_EngineeringJobSubsystem::HasMatchAuthority() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->HasAuthority();
	}

	return true;
}

bool UGP_EngineeringJobSubsystem::IsValidZoneSize(FIntPoint SizeCells) const
{
	return SizeCells.X > 0
		&& SizeCells.Y > 0
		&& SizeCells.X <= MaxZoneAxisCells
		&& SizeCells.Y <= MaxZoneAxisCells;
}

bool UGP_EngineeringJobSubsystem::SampleVoxelSurfaceZ(
	const FVector& WorldXY,
	float ReferenceZ,
	float& OutSurfaceZ) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const FVector Start(WorldXY.X, WorldXY.Y, ReferenceZ + GPEngineeringPrivate::TraceHalfExtentCm);
	const FVector End(WorldXY.X, WorldXY.Y, ReferenceZ - GPEngineeringPrivate::TraceHalfExtentCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GPEngineeringTerrainSample), true);

	const ECollisionChannel Channels[] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility };
	float BestDistance = 0.f;
	bool bHasBest = false;

	for (const ECollisionChannel Channel : Channels)
	{
		TArray<FHitResult> Hits;
		if (!World->LineTraceMultiByChannel(Hits, Start, End, Channel, Params))
		{
			continue;
		}

		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.bBlockingHit || !FMath::IsFinite(Hit.ImpactPoint.Z))
			{
				continue;
			}

			const AActor* HitActor = Hit.GetActor();
			if (HitActor != nullptr && HitActor->IsA(AGP_UnitBase::StaticClass()))
			{
				continue;
			}

			EGPVoxelWorldResolvePath Path = EGPVoxelWorldResolvePath::None;
			if (GPVoxelRuntimeProbeAdapter::ResolveVoxelWorldFromHit(Hit, Path) == nullptr)
			{
				continue;
			}

			if (!bHasBest || Hit.Distance < BestDistance)
			{
				OutSurfaceZ = Hit.ImpactPoint.Z;
				BestDistance = Hit.Distance;
				bHasBest = true;
			}
		}
	}

	return bHasBest;
}

void UGP_EngineeringJobSubsystem::EvaluateCell(
	UGP_BuildGridSubsystem* Grid,
	FIntPoint Cell,
	float TargetPlaneZ,
	FGP_TerrainLevelingCell& OutCell) const
{
	OutCell = FGP_TerrainLevelingCell();
	OutCell.Cell = Cell;

	const float CellSize = Grid->GetCellSize();
	const float Inset = CellSize * 0.25f;
	const FVector Center = Grid->CellToWorld(Cell, TargetPlaneZ);
	const FVector Offsets[] = {
		FVector::ZeroVector,
		FVector(Inset, Inset, 0.f),
		FVector(Inset, -Inset, 0.f),
		FVector(-Inset, Inset, 0.f),
		FVector(-Inset, -Inset, 0.f)
	};

	float SumZ = 0.f;
	float MinZ = 0.f;
	float MaxZ = 0.f;
	float MaxAbsDeviation = 0.f;
	int32 Hits = 0;
	bool bCenterHit = false;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
	{
		float SurfaceZ = 0.f;
		if (!SampleVoxelSurfaceZ(Center + Offsets[Index], TargetPlaneZ, SurfaceZ))
		{
			continue;
		}

		if (Index == 0)
		{
			bCenterHit = true;
		}

		if (Hits == 0)
		{
			MinZ = SurfaceZ;
			MaxZ = SurfaceZ;
		}
		else
		{
			MinZ = FMath::Min(MinZ, SurfaceZ);
			MaxZ = FMath::Max(MaxZ, SurfaceZ);
		}

		SumZ += SurfaceZ;
		MaxAbsDeviation = FMath::Max(MaxAbsDeviation, FMath::Abs(SurfaceZ - TargetPlaneZ));
		++Hits;
	}

	OutCell.SampleCount = Hits;
	if (!bCenterHit || Hits == 0)
	{
		OutCell.State = EGP_TerrainLevelingCellState::NoTerrain;
		return;
	}

	OutCell.MeanSurfaceZ = SumZ / static_cast<float>(Hits);
	OutCell.MinSurfaceZ = MinZ;
	OutCell.MaxSurfaceZ = MaxZ;
	OutCell.MaxAbsDeviationCm = MaxAbsDeviation;
	OutCell.State = MaxAbsDeviation > LevelHeightToleranceCm
		? EGP_TerrainLevelingCellState::NeedsLeveling
		: EGP_TerrainLevelingCellState::Level;
}

FGP_TerrainLevelingCreateResult UGP_EngineeringJobSubsystem::CreateTerrainLevelingJob(
	FIntPoint OriginCell,
	FIntPoint SizeCells,
	float TargetPlaneZ)
{
	FGP_TerrainLevelingCreateResult Result;
	Result.OriginCell = OriginCell;
	Result.SizeCells = SizeCells;
	Result.TargetPlaneZ = TargetPlaneZ;

	if (!HasMatchAuthority())
	{
		Result.Reason = EGP_EngineeringJobRejectReason::NoAuthority;
		return Result;
	}

	if (!FMath::IsFinite(TargetPlaneZ))
	{
		Result.Reason = EGP_EngineeringJobRejectReason::NonFiniteTarget;
		return Result;
	}

	if (!IsValidZoneSize(SizeCells))
	{
		Result.Reason = EGP_EngineeringJobRejectReason::InvalidSize;
		return Result;
	}

	UWorld* World = GetWorld();
	UGP_BuildGridSubsystem* Grid = World != nullptr
		? World->GetSubsystem<UGP_BuildGridSubsystem>()
		: nullptr;
	if (Grid == nullptr)
	{
		Result.Reason = EGP_EngineeringJobRejectReason::NoBuildGrid;
		return Result;
	}

	TArray<FIntPoint> Footprint;
	Grid->EnumerateFootprintCells(OriginCell, SizeCells, Footprint);
	Result.Cells.Reserve(Footprint.Num());

	bool bAnyNoTerrain = false;
	for (const FIntPoint& Cell : Footprint)
	{
		FGP_TerrainLevelingCell Evaluated;
		EvaluateCell(Grid, Cell, TargetPlaneZ, Evaluated);
		if (Evaluated.State == EGP_TerrainLevelingCellState::NoTerrain)
		{
			bAnyNoTerrain = true;
		}
		else if (Evaluated.State == EGP_TerrainLevelingCellState::NeedsLeveling)
		{
			++Result.NeedsLevelingCells;
		}
		else
		{
			++Result.LevelCells;
		}
		Result.Cells.Add(Evaluated);
	}

	Result.TotalCells = Result.Cells.Num();
	if (bAnyNoTerrain)
	{
		Result.Reason = EGP_EngineeringJobRejectReason::NoTerrain;
		UE_LOG(LogGPEngineering, Log,
			TEXT("CreateTerrainLevelingJob rejected reason=NoTerrain origin=(%d,%d) size=(%d,%d) targetZ=%.1f"),
			OriginCell.X, OriginCell.Y, SizeCells.X, SizeCells.Y, TargetPlaneZ);
		return Result;
	}

	if (Result.NeedsLevelingCells == 0)
	{
		Result.Reason = EGP_EngineeringJobRejectReason::NothingToLevel;
		UE_LOG(LogGPEngineering, Log,
			TEXT("CreateTerrainLevelingJob rejected reason=NothingToLevel origin=(%d,%d) size=(%d,%d) targetZ=%.1f level=%d"),
			OriginCell.X, OriginCell.Y, SizeCells.X, SizeCells.Y, TargetPlaneZ, Result.LevelCells);
		return Result;
	}

	FGP_TerrainLevelingJob Job;
	Job.JobId = FGuid::NewGuid();
	Job.Type = EGP_EngineeringJobType::TerrainLeveling;
	Job.State = EGP_EngineeringJobState::Planned;
	Job.OriginCell = OriginCell;
	Job.SizeCells = SizeCells;
	Job.TargetPlaneZ = TargetPlaneZ;
	Job.Cells = Result.Cells;
	Job.LevelCellCount = Result.LevelCells;
	Job.NeedsLevelingCellCount = Result.NeedsLevelingCells;
	Jobs.Add(Job.JobId, Job);

	Result.bAccepted = true;
	Result.JobId = Job.JobId;
	Result.Reason = EGP_EngineeringJobRejectReason::None;
	UE_LOG(LogGPEngineering, Log,
		TEXT("CreateTerrainLevelingJob accepted id=%s origin=(%d,%d) size=(%d,%d) targetZ=%.1f level=%d needs=%d"),
		*Job.JobId.ToString(),
		OriginCell.X, OriginCell.Y, SizeCells.X, SizeCells.Y, TargetPlaneZ,
		Result.LevelCells, Result.NeedsLevelingCells);
	return Result;
}

const FGP_TerrainLevelingJob* UGP_EngineeringJobSubsystem::FindTerrainLevelingJob(const FGuid& JobId) const
{
	return Jobs.Find(JobId);
}

void UGP_EngineeringJobSubsystem::GetTerrainLevelingJobs(TArray<FGP_TerrainLevelingJob>& OutJobs) const
{
	OutJobs.Reset();
	OutJobs.Reserve(Jobs.Num());
	for (const TPair<FGuid, FGP_TerrainLevelingJob>& Pair : Jobs)
	{
		OutJobs.Add(Pair.Value);
	}
}

bool UGP_EngineeringJobSubsystem::CancelTerrainLevelingJob(const FGuid& JobId)
{
	if (!HasMatchAuthority() || !JobId.IsValid())
	{
		return false;
	}

	const int32 Removed = Jobs.Remove(JobId);
	if (Removed > 0)
	{
		UE_LOG(LogGPEngineering, Log, TEXT("CancelTerrainLevelingJob id=%s"), *JobId.ToString());
	}
	return Removed > 0;
}
