// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include <limits>

#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engineering/GPEngineeringJobSubsystem.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Terrain/GPTerrainDeformationComponent.h"
#include "TimerManager.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPEngineeringLevelingContract, Log, All);

namespace GPEngineeringLevelingContract
{
	constexpr float ProbeOriginX = 62000.f;
	constexpr float ProbeOriginY = 0.f;
	constexpr float ProbeOriginZ = 200.f;
	constexpr float TickSeconds = 0.05f;
	constexpr int32 MaxReadyTicks = 200;
	constexpr int32 MaxResolveTicks = 120;

	struct FRunner
	{
		bool bActive = false;
		bool bFinished = false;
		bool bCancelled = false;
		uint64 ExecutionId = 0;
		int32 Failures = 0;
		int32 Stage = 0;
		int32 WaitTicks = 0;
		int32 DeformationCountAtCrater = 0;
		float PlaneZ = 0.f;
		bool bCraterRequested = false;
		FVector Surface = FVector::ZeroVector;
		FIntPoint SurfaceCell = FIntPoint::ZeroValue;
		FGuid StoredJobId;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> Probe;
		FTimerHandle Timer;
		FDelegateHandle CleanupHandle;

		void Fail(const TCHAR* Label)
		{
			++Failures;
			UE_LOG(LogGPEngineeringLevelingContract, Error,
				TEXT("gp.Engineering.RunLevelingPlanningContractTest FAIL: %s"), Label);
		}

		bool Expect(bool bCondition, const TCHAR* Label)
		{
			if (!bCondition)
			{
				Fail(Label);
				return false;
			}
			UE_LOG(LogGPEngineeringLevelingContract, Log,
				TEXT("gp.Engineering.RunLevelingPlanningContractTest PASS: %s"), Label);
			return true;
		}

		void Schedule()
		{
			UWorld* LiveWorld = World.Get();
			if (LiveWorld == nullptr)
			{
				Finish(true, TEXT("MissingWorld"));
				return;
			}
			LiveWorld->GetTimerManager().SetTimer(
				Timer,
				FTimerDelegate::CreateLambda([this]() { Tick(); }),
				TickSeconds,
				false);
		}

		void Finish(bool bInCancelled, const TCHAR* Reason)
		{
			if (bFinished)
			{
				return;
			}
			bFinished = true;
			bCancelled = bInCancelled;
			bActive = false;
			if (UWorld* LiveWorld = World.Get())
			{
				LiveWorld->GetTimerManager().ClearTimer(Timer);
			}
			if (AActor* LiveProbe = Probe.Get())
			{
				GPVoxelRuntimeProbeAdapter::DestroyProbeWorld(LiveProbe);
			}
			Probe = nullptr;
			if (CleanupHandle.IsValid())
			{
				FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
				CleanupHandle.Reset();
			}
			UE_LOG(LogGPEngineeringLevelingContract, Log,
				TEXT("gp.Engineering.RunLevelingPlanningContractTest: Complete Failures=%d Cancelled=%s%s%s"),
				Failures,
				bCancelled ? TEXT("true") : TEXT("false"),
				Reason != nullptr ? TEXT(" ") : TEXT(""),
				Reason != nullptr ? Reason : TEXT(""));
			GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, Reason);
		}

		bool RunValidation()
		{
			UWorld* LiveWorld = World.Get();
			UGP_BuildGridSubsystem* Grid = LiveWorld != nullptr
				? LiveWorld->GetSubsystem<UGP_BuildGridSubsystem>()
				: nullptr;
			UGP_EngineeringJobSubsystem* Jobs = LiveWorld != nullptr
				? LiveWorld->GetSubsystem<UGP_EngineeringJobSubsystem>()
				: nullptr;
			AGP_GameState* GameState = LiveWorld != nullptr ? LiveWorld->GetGameState<AGP_GameState>() : nullptr;
			if (!Expect(Grid != nullptr && Jobs != nullptr && GameState != nullptr, TEXT("Owner_Subsystems")))
			{
				return false;
			}

			TArray<FIntPoint> Cells;
			Grid->EnumerateFootprintCells(FIntPoint(3, 5), FIntPoint(2, 3), Cells);
			const bool bRect = Cells.Num() == 6
				&& Cells[0] == FIntPoint(3, 5)
				&& Cells[1] == FIntPoint(4, 5)
				&& Cells[5] == FIntPoint(4, 7);
			Expect(bRect, TEXT("A_RectangleEnumeration"));

			const ENetRole SavedRole = GameState->GetLocalRole();
			GameState->SetRole(ROLE_SimulatedProxy);
			const FGP_TerrainLevelingCreateResult ClientResult = Jobs->CreateTerrainLevelingJob(
				FIntPoint(0, 0), FIntPoint(2, 2), 0.f);
			GameState->SetRole(SavedRole);
			Expect(!ClientResult.bAccepted
				&& ClientResult.Reason == EGP_EngineeringJobRejectReason::NoAuthority
				&& Jobs->GetTerrainLevelingJobCount() == 0,
				TEXT("C_ClientRejected"));

			const FGP_TerrainLevelingCreateResult ZeroSize = Jobs->CreateTerrainLevelingJob(
				FIntPoint(0, 0), FIntPoint(0, 4), 0.f);
			const FGP_TerrainLevelingCreateResult HugeSize = Jobs->CreateTerrainLevelingJob(
				FIntPoint(0, 0), FIntPoint(UGP_EngineeringJobSubsystem::MaxZoneAxisCells + 1, 1), 0.f);
			Expect(!ZeroSize.bAccepted && ZeroSize.Reason == EGP_EngineeringJobRejectReason::InvalidSize
				&& !HugeSize.bAccepted && HugeSize.Reason == EGP_EngineeringJobRejectReason::InvalidSize,
				TEXT("D_InvalidSize"));

			const FGP_TerrainLevelingCreateResult NanTarget = Jobs->CreateTerrainLevelingJob(
				FIntPoint(0, 0), FIntPoint(2, 2), std::numeric_limits<float>::quiet_NaN());
			Expect(!NanTarget.bAccepted && NanTarget.Reason == EGP_EngineeringJobRejectReason::NonFiniteTarget,
				TEXT("E_NonFiniteTargetRejected"));
			return Failures == 0;
		}

		void Tick()
		{
			if (bFinished)
			{
				return;
			}
			UWorld* LiveWorld = World.Get();
			if (LiveWorld == nullptr || GPContractTestCoordinator::IsWorldTearingDown(LiveWorld))
			{
				Finish(true, TEXT("WorldGone"));
				return;
			}

			UGP_EngineeringJobSubsystem* Jobs = LiveWorld->GetSubsystem<UGP_EngineeringJobSubsystem>();
			UGP_BuildGridSubsystem* Grid = LiveWorld->GetSubsystem<UGP_BuildGridSubsystem>();
			AGP_GameState* GameState = LiveWorld->GetGameState<AGP_GameState>();
			UGP_TerrainDeformationComponent* Terrain = GameState != nullptr
				? GameState->GetTerrainDeformationComponent()
				: nullptr;
			if (Jobs == nullptr || Grid == nullptr || Terrain == nullptr)
			{
				Finish(true, TEXT("MissingService"));
				return;
			}

			if (Stage == 0)
			{
				if (AActor* Existing = GPVoxelRuntimeProbeAdapter::FindExistingProbe(LiveWorld))
				{
					GPVoxelRuntimeProbeAdapter::DestroyProbeWorld(Existing);
				}
				AActor* Spawned = GPVoxelRuntimeProbeAdapter::SpawnConfiguredProbeWorld(
					LiveWorld, FVector(ProbeOriginX, ProbeOriginY, ProbeOriginZ));
				if (!Expect(Spawned != nullptr && GPVoxelRuntimeProbeAdapter::CreateWorldIfNeeded(Spawned),
					TEXT("SpawnProbe")))
				{
					Finish(false, nullptr);
					return;
				}
				Probe = Spawned;
				Stage = 1;
				WaitTicks = 0;
				Schedule();
				return;
			}

			AActor* LiveProbe = Probe.Get();
			if (LiveProbe == nullptr)
			{
				Finish(true, TEXT("ProbeGone"));
				return;
			}

			if (Stage == 1)
			{
				++WaitTicks;
				const bool bReady = GPVoxelRuntimeProbeAdapter::IsCreated(LiveProbe)
					&& GPVoxelRuntimeProbeAdapter::CountProcMeshComponents(LiveProbe) > 0
					&& GPVoxelRuntimeProbeAdapter::IsMeshIdle(LiveProbe);
				if (!bReady)
				{
					if (WaitTicks >= MaxReadyTicks)
					{
						Finish(true, TEXT("ReadyTimeout"));
						return;
					}
					Schedule();
					return;
				}
				Surface = GPVoxelRuntimeProbeAdapter::VoxelToWorld(LiveProbe, FIntVector(0, 0, -1));
				SurfaceCell = Grid->WorldToCell(Surface);
				Stage = 2;
				WaitTicks = 0;
				Schedule();
				return;
			}

			if (Stage == 2)
			{
				const FGP_TerrainLevelingCreateResult ProbeSample = Jobs->CreateTerrainLevelingJob(
					SurfaceCell, FIntPoint(1, 1), Surface.Z);
				if (ProbeSample.Reason == EGP_EngineeringJobRejectReason::NoTerrain && WaitTicks < MaxResolveTicks)
				{
					++WaitTicks;
					Schedule();
					return;
				}
				if (ProbeSample.Cells.Num() != 1 || ProbeSample.Cells[0].SampleCount <= 0)
				{
					Expect(false, TEXT("F_SurfaceSample"));
					Finish(false, nullptr);
					return;
				}
				if (ProbeSample.bAccepted)
				{
					Jobs->CancelTerrainLevelingJob(ProbeSample.JobId);
				}
				PlaneZ = ProbeSample.Cells[0].MeanSurfaceZ;

				const FGP_TerrainLevelingCreateResult Flat = Jobs->CreateTerrainLevelingJob(
					SurfaceCell, FIntPoint(2, 2), PlaneZ);
				const bool bFlat = !Flat.bAccepted
					&& Flat.Reason == EGP_EngineeringJobRejectReason::NothingToLevel
					&& Flat.TotalCells == 4
					&& Flat.LevelCells == 4
					&& Flat.NeedsLevelingCells == 0
					&& Jobs->GetTerrainLevelingJobCount() == 0;
				if (!Expect(bFlat, TEXT("F_FlatAtTargetIsLevel"))
					|| !Expect(bFlat, TEXT("H_AllLevelNothingToLevel")))
				{
					UE_LOG(LogGPEngineeringLevelingContract, Error,
						TEXT("gp.Engineering.RunLevelingPlanningContractTest detail flat reason=%d total=%d level=%d needs=%d planeZ=%.1f"),
						static_cast<int32>(Flat.Reason), Flat.TotalCells, Flat.LevelCells, Flat.NeedsLevelingCells, PlaneZ);
					Finish(false, nullptr);
					return;
				}

				const float RaisedTarget = PlaneZ + (UGP_EngineeringJobSubsystem::LevelHeightToleranceCm + 25.f);
				const FGP_TerrainLevelingCreateResult Raised = Jobs->CreateTerrainLevelingJob(
					SurfaceCell, FIntPoint(1, 1), RaisedTarget);
				const bool bRaised = Raised.bAccepted
					&& Raised.NeedsLevelingCells == 1
					&& FMath::IsNearlyEqual(Raised.TargetPlaneZ, RaisedTarget, 0.01f)
					&& Jobs->FindTerrainLevelingJob(Raised.JobId) != nullptr
					&& FMath::IsNearlyEqual(Jobs->FindTerrainLevelingJob(Raised.JobId)->TargetPlaneZ, RaisedTarget, 0.01f);
				if (!Expect(bRaised, TEXT("B_AuthorityCreate"))
					|| !Expect(bRaised, TEXT("E_TargetPlanePreserved")))
				{
					Finish(false, nullptr);
					return;
				}
				Expect(Jobs->CancelTerrainLevelingJob(Raised.JobId) && Jobs->GetTerrainLevelingJobCount() == 0,
					TEXT("I_CancelRemovesJob"));

				Stage = 3;
				WaitTicks = 0;
				Schedule();
				return;
			}

			if (Stage == 3)
			{
				if (!bCraterRequested)
				{
					FGP_TerrainDeformationRequest Request;
					const FVector Impact = Grid->CellToWorld(SurfaceCell, PlaneZ);
					Request.WorldLocation = Impact;
					Request.RadiusCm = 400.f;
					Request.DepthCm = 80.f;
					Request.Profile = EGP_TerrainDeformationProfile::ShallowSphereCap;
					Request.SourceIdentity = TEXT("LevelingContract");
					const int32 Before = Terrain->GetEventLog().Num();
					const FGP_TerrainDeformationResult Crater = Terrain->RequestDeformation(Request);
					if (!Crater.bAccepted && Crater.Reason == EGP_TerrainDeformationRejectReason::NoVoxelWorld
						&& WaitTicks < MaxResolveTicks)
					{
						++WaitTicks;
						Schedule();
						return;
					}
					if (!Expect(Crater.bAccepted && Terrain->GetEventLog().Num() == Before + 1, TEXT("G_CraterSetup")))
					{
						Finish(false, nullptr);
						return;
					}
					bCraterRequested = true;
					DeformationCountAtCrater = Terrain->GetEventLog().Num();
					WaitTicks = 0;
					Schedule();
					return;
				}

				++WaitTicks;
				const FGP_TerrainLevelingCreateResult CraterCell = Jobs->CreateTerrainLevelingJob(
					SurfaceCell, FIntPoint(1, 1), PlaneZ);
				if (!CraterCell.bAccepted
					&& CraterCell.Reason == EGP_EngineeringJobRejectReason::NothingToLevel
					&& WaitTicks < MaxResolveTicks)
				{
					Schedule();
					return;
				}

				const bool bNeeds = CraterCell.bAccepted
					&& CraterCell.NeedsLevelingCells == 1
					&& CraterCell.LevelCells == 0
					&& Terrain->GetEventLog().Num() == DeformationCountAtCrater;
				if (!Expect(bNeeds, TEXT("G_CraterNeedsLeveling"))
					|| !Expect(bNeeds, TEXT("J_NoDeformationFromPlanning")))
				{
					UE_LOG(LogGPEngineeringLevelingContract, Error,
						TEXT("gp.Engineering.RunLevelingPlanningContractTest detail crater accepted=%s reason=%d needs=%d events=%d expectedEvents=%d"),
						CraterCell.bAccepted ? TEXT("true") : TEXT("false"),
						static_cast<int32>(CraterCell.Reason),
						CraterCell.NeedsLevelingCells,
						Terrain->GetEventLog().Num(),
						DeformationCountAtCrater);
					Finish(false, nullptr);
					return;
				}

				StoredJobId = CraterCell.JobId;
				const FGP_TerrainLevelingJob* Found = Jobs->FindTerrainLevelingJob(StoredJobId);
				TArray<FGP_TerrainLevelingJob> Listed;
				Jobs->GetTerrainLevelingJobs(Listed);
				const bool bPersisted = Found != nullptr
					&& Found->State == EGP_EngineeringJobState::Planned
					&& Found->Type == EGP_EngineeringJobType::TerrainLeveling
					&& Found->Cells.Num() == 1
					&& Listed.Num() == 1
					&& Listed[0].JobId == StoredJobId;
				Expect(bPersisted, TEXT("I_JobPersists"));
				Finish(false, nullptr);
			}
		}
	};

	static FRunner GRunner;

	static void Run(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPEngineeringLevelingContract, Warning,
				TEXT("gp.Engineering.RunLevelingPlanningContractTest: missing world or client"));
			return;
		}
		if (GRunner.bActive)
		{
			UE_LOG(LogGPEngineeringLevelingContract, Warning,
				TEXT("gp.Engineering.RunLevelingPlanningContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("EngineeringLevelingPlanning"), TEXT("Engineering"), Token))
		{
			return;
		}

		GRunner = FRunner();
		GRunner.bActive = true;
		GRunner.ExecutionId = Token.ExecutionId;
		GRunner.World = World;
		GRunner.CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* CleanupWorld, bool, bool)
		{
			if (GRunner.bActive && GRunner.World.Get() == CleanupWorld)
			{
				GRunner.Finish(true, TEXT("WorldCleanup"));
			}
		});
		if (!GRunner.RunValidation())
		{
			GRunner.Finish(false, nullptr);
			return;
		}
		GRunner.Tick();
	}

	static FAutoConsoleCommandWithWorldAndArgs GCommand(
		TEXT("gp.Engineering.RunLevelingPlanningContractTest"),
		TEXT("Terrain leveling planning contracts. Does not deform except the crater setup inside the test."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));
}

#endif
