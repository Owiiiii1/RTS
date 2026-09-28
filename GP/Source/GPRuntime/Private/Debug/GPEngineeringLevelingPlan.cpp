// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engineering/GPEngineeringJobSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPEngineeringPlan, Log, All);

namespace GPEngineeringPlanDebug
{
	constexpr float CursorTraceDistanceCm = 1000000.f;
	constexpr float DrawSeconds = 8.f;
	constexpr int32 MessageKey = 0x47504531;

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

	static void Screen(const FColor& Color, const FString& Text)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(MessageKey, DrawSeconds, Color, Text);
		}
	}

	static bool ClosestChannelHit(
		UWorld* World,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params,
		FHitResult& InOutBest,
		bool& bInOutHasBest)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params) || !Hit.bBlockingHit)
		{
			return false;
		}
		if (!bInOutHasBest || Hit.Distance < InOutBest.Distance)
		{
			InOutBest = Hit;
			bInOutHasBest = true;
		}
		return true;
	}

	static bool TraceCursor(UWorld* World, APlayerController* PlayerController, FHitResult& OutHit)
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!PlayerController->GetMousePosition(MouseX, MouseY))
		{
			return false;
		}

		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;
		if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
		{
			return false;
		}

		const FVector End = Origin + Direction * CursorTraceDistanceCm;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GPEngineeringPlanCursor), true);
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			Params.AddIgnoredActor(Pawn);
		}

		bool bHasBest = false;
		ClosestChannelHit(World, Origin, End, ECC_Visibility, Params, OutHit, bHasBest);
		ClosestChannelHit(World, Origin, End, ECC_WorldStatic, Params, OutHit, bHasBest);
		ClosestChannelHit(World, Origin, End, ECC_WorldDynamic, Params, OutHit, bHasBest);
		return bHasBest;
	}

	static void DrawCells(UWorld* World, UGP_BuildGridSubsystem* Grid, const FGP_TerrainLevelingCreateResult& Result)
	{
		if (Grid == nullptr)
		{
			return;
		}

		const float Half = Grid->GetCellSize() * 0.45f;
		for (const FGP_TerrainLevelingCell& Cell : Result.Cells)
		{
			const float DrawZ = Cell.SampleCount > 0 ? Cell.MeanSurfaceZ : Result.TargetPlaneZ;
			const FVector Center = Grid->CellToWorld(Cell.Cell, DrawZ);
			FColor Color = FColor(140, 140, 140);
			if (Cell.State == EGP_TerrainLevelingCellState::NeedsLeveling)
			{
				Color = FColor::Yellow;
			}
			else if (Cell.State == EGP_TerrainLevelingCellState::NoTerrain)
			{
				Color = FColor::Red;
			}
			DrawDebugBox(World, Center, FVector(Half, Half, 6.f), Color, false, DrawSeconds, 0, 2.f);
		}
	}

	static void PlanUnderCursor(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
		{
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPEngineeringPlan, Warning,
				TEXT("gp.Engineering.PlanLevelingUnderCursor: client refused"));
			Screen(FColor::Red, TEXT("leveling plan refused — authority viewport"));
			return;
		}

		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			UE_LOG(LogGPEngineeringPlan, Warning,
				TEXT("gp.Engineering.PlanLevelingUnderCursor: no local player"));
			return;
		}

		int32 Width = 4;
		int32 Height = 4;
		if (Args.Num() > 0 && !Args[0].IsEmpty())
		{
			Width = FCString::Atoi(*Args[0]);
		}
		if (Args.Num() > 1 && !Args[1].IsEmpty())
		{
			Height = FCString::Atoi(*Args[1]);
		}

		FHitResult CursorHit;
		if (!TraceCursor(World, PlayerController, CursorHit))
		{
			UE_LOG(LogGPEngineeringPlan, Warning,
				TEXT("gp.Engineering.PlanLevelingUnderCursor: cursor ray hit nothing"));
			Screen(FColor::Red, TEXT("cursor ray hit nothing"));
			return;
		}

		EGPVoxelWorldResolvePath ResolvePath = EGPVoxelWorldResolvePath::None;
		if (GPVoxelRuntimeProbeAdapter::ResolveVoxelWorldFromHit(CursorHit, ResolvePath) == nullptr)
		{
			UE_LOG(LogGPEngineeringPlan, Warning,
				TEXT("gp.Engineering.PlanLevelingUnderCursor: cursor is not voxel terrain"));
			Screen(FColor::Red, TEXT("not voxel terrain"));
			return;
		}

		UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
		UGP_EngineeringJobSubsystem* Jobs = World->GetSubsystem<UGP_EngineeringJobSubsystem>();
		if (Grid == nullptr || Jobs == nullptr)
		{
			UE_LOG(LogGPEngineeringPlan, Warning,
				TEXT("gp.Engineering.PlanLevelingUnderCursor: missing grid or job subsystem"));
			return;
		}

		const FIntPoint Size(Width, Height);
		const FIntPoint Origin = Grid->SnapOriginCell(CursorHit.ImpactPoint, Size);
		const FGP_TerrainLevelingCreateResult Result = Jobs->CreateTerrainLevelingJob(
			Origin, Size, CursorHit.ImpactPoint.Z);

		UE_LOG(LogGPEngineeringPlan, Log,
			TEXT("gp.Engineering.PlanLevelingUnderCursor accepted=%s id=%s origin=(%d,%d) size=(%d,%d) targetZ=%.1f level=%d needs=%d reason=%s"),
			Result.bAccepted ? TEXT("true") : TEXT("false"),
			Result.JobId.IsValid() ? *Result.JobId.ToString() : TEXT("none"),
			Result.OriginCell.X, Result.OriginCell.Y,
			Result.SizeCells.X, Result.SizeCells.Y,
			Result.TargetPlaneZ,
			Result.LevelCells,
			Result.NeedsLevelingCells,
			ReasonName(Result.Reason));

		DrawCells(World, Grid, Result);
		const FColor Color = Result.bAccepted ? FColor::Yellow : FColor(180, 180, 180);
		Screen(Color, FString::Printf(
			TEXT("level plan %s  level=%d needs=%d"),
			ReasonName(Result.Reason), Result.LevelCells, Result.NeedsLevelingCells));
	}

	static void DumpJobs(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			return;
		}

		UGP_EngineeringJobSubsystem* Jobs = World->GetSubsystem<UGP_EngineeringJobSubsystem>();
		if (Jobs == nullptr)
		{
			UE_LOG(LogGPEngineeringPlan, Warning, TEXT("gp.Engineering.DumpJobs: no subsystem"));
			return;
		}

		TArray<FGP_TerrainLevelingJob> Listed;
		Jobs->GetTerrainLevelingJobs(Listed);
		UE_LOG(LogGPEngineeringPlan, Log, TEXT("gp.Engineering.DumpJobs count=%d"), Listed.Num());
		for (const FGP_TerrainLevelingJob& Job : Listed)
		{
			UE_LOG(LogGPEngineeringPlan, Log,
				TEXT("gp.Engineering.DumpJobs id=%s state=%s origin=(%d,%d) size=(%d,%d) targetZ=%.1f level=%d needs=%d"),
				*Job.JobId.ToString(),
				Job.State == EGP_EngineeringJobState::Planned ? TEXT("Planned") : TEXT("Completed"),
				Job.OriginCell.X, Job.OriginCell.Y,
				Job.SizeCells.X, Job.SizeCells.Y,
				Job.TargetPlaneZ,
				Job.LevelCellCount,
				Job.NeedsLevelingCellCount);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GPlan(
		TEXT("gp.Engineering.PlanLevelingUnderCursor"),
		TEXT("Authority debug: plan a BuildGrid leveling zone under the cursor. Args: WidthCells HeightCells. Defaults 4 4. Does not deform terrain."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PlanUnderCursor));

	static FAutoConsoleCommandWithWorldAndArgs GDump(
		TEXT("gp.Engineering.DumpJobs"),
		TEXT("Log planned terrain leveling jobs on this world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpJobs));
}

#endif
