// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVoxelRuntimeVisualProbe, Log, All);

namespace GPVoxelRuntimeVisualProbe
{
	constexpr float CraterRadiusCm = 300.f;
	constexpr float IsolatedFallbackX = 25000.f;

	static FVector ChooseSpawnLocation(UWorld* World)
	{
		if (World != nullptr)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 2500.f;
					Location.Z = Pawn->GetActorLocation().Z;
					return Location;
				}
			}
		}
		return FVector(IsolatedFallbackX, 0.f, 200.f);
	}

	static void SpawnRuntimeProbe(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Warning,
				TEXT("gp.Voxel.SpawnRuntimeProbe: missing world"));
			return;
		}

		if (AActor* Existing = GPVoxelRuntimeProbeAdapter::FindExistingProbe(World))
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Log,
				TEXT("gp.Voxel.SpawnRuntimeProbe: existing probe at %s"),
				*Existing->GetActorLocation().ToString());
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					INDEX_NONE, 8.f, FColor::Cyan,
					FString::Printf(TEXT("GP Voxel probe already at %s"), *Existing->GetActorLocation().ToCompactString()));
			}
			return;
		}

		const FVector Location = ChooseSpawnLocation(World);
		AActor* Probe = GPVoxelRuntimeProbeAdapter::SpawnConfiguredProbeWorld(World, Location);
		if (Probe == nullptr)
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Error,
				TEXT("gp.Voxel.SpawnRuntimeProbe FAIL: spawn returned null"));
			return;
		}
		if (!GPVoxelRuntimeProbeAdapter::CreateWorldIfNeeded(Probe))
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Error,
				TEXT("gp.Voxel.SpawnRuntimeProbe FAIL: CreateWorld"));
			return;
		}

		UE_LOG(LogGPVoxelRuntimeVisualProbe, Log,
			TEXT("gp.Voxel.SpawnRuntimeProbe: spawned UVoxelFlatGenerator AVoxelWorld at %s VoxelSize=100 RenderOctreeDepth=1. Then: gp.Voxel.ApplyProbeCrater"),
			*Probe->GetActorLocation().ToString());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				INDEX_NONE, 12.f, FColor::Green,
				FString::Printf(TEXT("GP Voxel probe spawned at %s — run gp.Voxel.ApplyProbeCrater"),
					*Probe->GetActorLocation().ToCompactString()));
		}
		DrawDebugBox(World, Probe->GetActorLocation(), FVector(800.f, 800.f, 50.f), FColor::Cyan, false, 20.f, 0, 8.f);
	}

	static void ApplyProbeCrater(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Warning,
				TEXT("gp.Voxel.ApplyProbeCrater: missing world"));
			return;
		}

		AActor* Probe = GPVoxelRuntimeProbeAdapter::FindExistingProbe(World);
		if (Probe == nullptr)
		{
			UE_LOG(LogGPVoxelRuntimeVisualProbe, Warning,
				TEXT("gp.Voxel.ApplyProbeCrater: no probe — run gp.Voxel.SpawnRuntimeProbe first"));
			return;
		}
		if (!GPVoxelRuntimeProbeAdapter::IsCreated(Probe))
		{
			GPVoxelRuntimeProbeAdapter::CreateWorldIfNeeded(Probe);
		}

		FGPVoxelSphereSubtractRequest Request;
		Request.WorldLocation = Probe->GetActorLocation();
		Request.RadiusCm = CraterRadiusCm;
		FGPVoxelIntBoxReport Edited;
		const bool bOk = GPVoxelRuntimeProbeAdapter::ApplySphereSubtract(Probe, Request, Edited);
		UE_LOG(LogGPVoxelRuntimeVisualProbe, Log,
			TEXT("gp.Voxel.ApplyProbeCrater: ok=%s center=%s radius=%.1fcm EditedBounds valid=%s infinite=%s (%d/%d, %d/%d, %d/%d)"),
			bOk ? TEXT("true") : TEXT("false"),
			*Request.WorldLocation.ToString(),
			Request.RadiusCm,
			Edited.bValid ? TEXT("true") : TEXT("false"),
			Edited.bInfinite ? TEXT("true") : TEXT("false"),
			Edited.Min.X, Edited.Max.X, Edited.Min.Y, Edited.Max.Y, Edited.Min.Z, Edited.Max.Z);

		DrawDebugSphere(World, Request.WorldLocation, CraterRadiusCm, 24, FColor::Orange, false, 20.f, 0, 4.f);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				INDEX_NONE, 10.f, FColor::Orange,
				TEXT("GP Voxel crater applied (RemoveSphere). Inspect mesh + walk/trace the hole."));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GSpawnRuntimeProbe(
		TEXT("gp.Voxel.SpawnRuntimeProbe"),
		TEXT("Spawn a small runtime UVoxelFlatGenerator probe (does not save the map)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnRuntimeProbe));

	static FAutoConsoleCommandWithWorldAndArgs GApplyProbeCrater(
		TEXT("gp.Voxel.ApplyProbeCrater"),
		TEXT("Apply RemoveSphere crater on the runtime voxel probe."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ApplyProbeCrater));
}

#endif
