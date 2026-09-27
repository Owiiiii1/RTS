// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#include <limits>

#if !UE_BUILD_SHIPPING

#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Terrain/GPTerrainDeformationComponent.h"
#include "TimerManager.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPTerrainDeformationContract, Log, All);

namespace GPTerrainDeformationContract
{
	constexpr float ProbeOriginX = 45000.f;
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
		int32 Broadcasts = 0;
		FVector Surface = FVector::ZeroVector;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> Probe;
		TWeakObjectPtr<UGP_TerrainDeformationComponent> Terrain;
		FDelegateHandle AppliedHandle;
		FTimerHandle Timer;
		FDelegateHandle CleanupHandle;

		void Fail(const TCHAR* Label)
		{
			++Failures;
			UE_LOG(LogGPTerrainDeformationContract, Error,
				TEXT("gp.Terrain.RunDeformationContractTest FAIL: %s"), Label);
		}

		bool Expect(bool bCondition, const TCHAR* Label)
		{
			if (!bCondition)
			{
				Fail(Label);
				return false;
			}
			UE_LOG(LogGPTerrainDeformationContract, Log,
				TEXT("gp.Terrain.RunDeformationContractTest PASS: %s"), Label);
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
			if (UGP_TerrainDeformationComponent* Component = Terrain.Get())
			{
				Component->OnTerrainDeformationApplied.Remove(AppliedHandle);
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

			UE_LOG(LogGPTerrainDeformationContract, Log,
				TEXT("gp.Terrain.RunDeformationContractTest: Complete Failures=%d Cancelled=%s%s%s"),
				Failures,
				bCancelled ? TEXT("true") : TEXT("false"),
				Reason != nullptr ? TEXT(" ") : TEXT(""),
				Reason != nullptr ? Reason : TEXT(""));

			GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, Reason);
		}

		FGP_TerrainDeformationRequest MakeRequest(const FVector& Location, float Radius, float Depth) const
		{
			FGP_TerrainDeformationRequest Request;
			Request.WorldLocation = Location;
			Request.RadiusCm = Radius;
			Request.DepthCm = Depth;
			Request.Profile = EGP_TerrainDeformationProfile::ShallowSphereCap;
			Request.Seed = 11;
			Request.RotationDegrees = 25.f;
			Request.SurfaceScar = EGP_TerrainSurfaceScarType::None;
			Request.SourceIdentity = TEXT("Contract");
			return Request;
		}

		bool RunValidation()
		{
			UWorld* LiveWorld = World.Get();
			AGP_GameState* GameState = LiveWorld != nullptr ? LiveWorld->GetGameState<AGP_GameState>() : nullptr;
			UGP_TerrainDeformationComponent* Component = GameState != nullptr
				? GameState->GetTerrainDeformationComponent()
				: nullptr;
			if (!Expect(Component != nullptr, TEXT("Owner_GameStateComponent")))
			{
				return false;
			}
			Terrain = Component;
			AppliedHandle = Component->OnTerrainDeformationApplied.AddLambda([this](const FGP_TerrainDeformationEvent&)
			{
				++Broadcasts;
			});

			Expect(IConsoleManager::Get().FindConsoleObject(TEXT("gp.Voxel.CraterUnderCursor")) != nullptr,
				TEXT("H_DebugCommandRegistered"));

			FGP_TerrainDeformationRequest NanRequest = MakeRequest(FVector(0.f, 0.f, 0.f), 400.f, 80.f);
			NanRequest.WorldLocation.X = std::numeric_limits<float>::quiet_NaN();
			const FGP_TerrainDeformationResult NanResult = Component->RequestDeformation(NanRequest);
			Expect(!NanResult.bAccepted && NanResult.Reason == EGP_TerrainDeformationRejectReason::NonFinite,
				TEXT("C_NonFiniteLocation"));

			FGP_TerrainDeformationRequest InfRequest = MakeRequest(FVector(0.f, 0.f, 0.f), 400.f, 80.f);
			InfRequest.RadiusCm = std::numeric_limits<float>::infinity();
			const FGP_TerrainDeformationResult InfResult = Component->RequestDeformation(InfRequest);
			Expect(!InfResult.bAccepted && InfResult.Reason == EGP_TerrainDeformationRejectReason::NonFinite,
				TEXT("C_NonFiniteRadius"));

			const FGP_TerrainDeformationResult RadiusResult = Component->RequestDeformation(
				MakeRequest(FVector(0.f, 0.f, 0.f), -5.f, 80.f));
			Expect(!RadiusResult.bAccepted && RadiusResult.Reason == EGP_TerrainDeformationRejectReason::InvalidRadius,
				TEXT("C_InvalidRadius"));

			const FGP_TerrainDeformationResult DepthResult = Component->RequestDeformation(
				MakeRequest(FVector(0.f, 0.f, 0.f), 400.f, 0.f));
			Expect(!DepthResult.bAccepted && DepthResult.Reason == EGP_TerrainDeformationRejectReason::InvalidDepth,
				TEXT("C_InvalidDepth"));

			FGP_TerrainDeformationRequest ProfileRequest = MakeRequest(FVector(0.f, 0.f, 0.f), 400.f, 80.f);
			ProfileRequest.Profile = static_cast<EGP_TerrainDeformationProfile>(255);
			const FGP_TerrainDeformationResult ProfileResult = Component->RequestDeformation(ProfileRequest);
			Expect(!ProfileResult.bAccepted
				&& ProfileResult.Reason == EGP_TerrainDeformationRejectReason::UnsupportedProfile,
				TEXT("C_UnsupportedProfile"));

			const FGP_TerrainDeformationResult MissingWorld = Component->RequestDeformation(
				MakeRequest(FVector(1.0e8f, 1.0e8f, 1.0e8f), 400.f, 80.f));
			Expect(!MissingWorld.bAccepted && MissingWorld.Reason == EGP_TerrainDeformationRejectReason::NoVoxelWorld,
				TEXT("C_NoVoxelWorld"));

			const ENetRole SavedRole = GameState->GetLocalRole();
			GameState->SetRole(ROLE_SimulatedProxy);
			const FGP_TerrainDeformationResult ClientResult = Component->RequestDeformation(
				MakeRequest(FVector(0.f, 0.f, 0.f), 400.f, 80.f));
			GameState->SetRole(SavedRole);
			Expect(!ClientResult.bAccepted && ClientResult.Reason == EGP_TerrainDeformationRejectReason::NoAuthority,
				TEXT("B_ClientRejected"));
			Expect(Component->GetEventLog().Num() == 0 && Broadcasts == 0, TEXT("B_LogUntouched"));
			return Failures == 0;
		}

		void Tick()
		{
			if (bFinished)
			{
				return;
			}
			UWorld* LiveWorld = World.Get();
			UGP_TerrainDeformationComponent* Component = Terrain.Get();
			if (LiveWorld == nullptr || Component == nullptr || GPContractTestCoordinator::IsWorldTearingDown(LiveWorld))
			{
				Finish(true, TEXT("WorldGone"));
				return;
			}

			if (Stage == 0)
			{
				if (AActor* Existing = GPVoxelRuntimeProbeAdapter::FindExistingProbe(LiveWorld))
				{
					GPVoxelRuntimeProbeAdapter::DestroyProbeWorld(Existing);
				}
				const FVector Origin(ProbeOriginX, ProbeOriginY, ProbeOriginZ);
				AActor* Spawned = GPVoxelRuntimeProbeAdapter::SpawnConfiguredProbeWorld(LiveWorld, Origin);
				if (!Expect(Spawned != nullptr, TEXT("D_SpawnProbe"))
					|| !Expect(GPVoxelRuntimeProbeAdapter::CreateWorldIfNeeded(Spawned), TEXT("D_CreateWorld")))
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
				Expect(true, TEXT("D_ProbeReady"));
				Stage = 2;
				WaitTicks = 0;
				Schedule();
				return;
			}

			if (Stage == 2)
			{
				++WaitTicks;
				const FGP_TerrainDeformationResult Accepted = Component->RequestDeformation(
					MakeRequest(Surface, 400.f, 80.f));
				if (!Accepted.bAccepted && Accepted.Reason == EGP_TerrainDeformationRejectReason::NoVoxelWorld
					&& WaitTicks < MaxResolveTicks)
				{
					Schedule();
					return;
				}

				const bool bShallow = Accepted.bAccepted
					&& Accepted.SequenceId == 1
					&& Accepted.AppliedRadiusCm == 400.f
					&& Accepted.AppliedDepthCm == 80.f
					&& FMath::IsNearlyEqual(Accepted.EditCenter.Z, Surface.Z + 320.f, 0.1f)
					&& Accepted.EditedBounds.bValid
					&& Accepted.ResolvedVoxelWorld.Get() == LiveProbe
					&& Component->GetEventLog().Num() == 1
					&& Component->HasAppliedSequence(1)
					&& Component->GetEventLog()[0].Seed == 11
					&& FMath::IsNearlyEqual(Component->GetEventLog()[0].RotationDegrees, 25.f)
					&& Component->GetEventLog()[0].Profile == EGP_TerrainDeformationProfile::ShallowSphereCap
					&& Broadcasts == 1;
				if (!Expect(bShallow, TEXT("A_AuthorityShallowAccepted")))
				{
					UE_LOG(LogGPTerrainDeformationContract, Error,
						TEXT("gp.Terrain.RunDeformationContractTest detail accepted=%s seq=%d reason=%d radius=%.1f depth=%.1f editZ=%.1f surfaceZ=%.1f bounds=%s world=%s log=%d broadcasts=%d"),
						Accepted.bAccepted ? TEXT("true") : TEXT("false"),
						Accepted.SequenceId,
						static_cast<int32>(Accepted.Reason),
						Accepted.AppliedRadiusCm,
						Accepted.AppliedDepthCm,
						Accepted.EditCenter.Z,
						Surface.Z,
						Accepted.EditedBounds.bValid ? TEXT("true") : TEXT("false"),
						Accepted.ResolvedVoxelWorld != nullptr ? *Accepted.ResolvedVoxelWorld->GetName() : TEXT("none"),
						Component->GetEventLog().Num(),
						Broadcasts);
					Finish(false, nullptr);
					return;
				}

				const int32 BroadcastsBeforeReplay = Broadcasts;
				Component->ApplyPendingAuthoritativeEvents();
				Expect(Broadcasts == BroadcastsBeforeReplay && Component->GetAppliedCount() == 1,
					TEXT("G_ListenServerApplyOnce"));

				const FGP_TerrainDeformationResult Clamped = Component->RequestDeformation(
					MakeRequest(Surface, 2000.f, 9000.f));
				const bool bClamped = Clamped.bAccepted
					&& Clamped.SequenceId == 2
					&& Clamped.AppliedRadiusCm == 1500.f
					&& Clamped.AppliedDepthCm == 1500.f
					&& FMath::IsNearlyEqual(Clamped.EditCenter.Z, Surface.Z, 0.1f)
					&& Clamped.EditedBounds.bValid
					&& Component->GetEventLog().Num() == 2
					&& Component->GetEventLog()[1].SequenceId == 2
					&& Component->GetEventLog()[1].SequenceId > Component->GetEventLog()[0].SequenceId
					&& Broadcasts == 2;
				if (!Expect(bClamped, TEXT("C_DepthAndRadiusClamped"))
					|| !Expect(bClamped, TEXT("E_SequenceMonotonic")))
				{
					Finish(false, nullptr);
					return;
				}

				Component->ApplyPendingAuthoritativeEvents();
				Component->ApplyPendingAuthoritativeEvents();
				Expect(Broadcasts == 2 && Component->GetAppliedCount() == 2
					&& Component->HasAppliedSequence(1) && Component->HasAppliedSequence(2),
					TEXT("F_DuplicateReplaySkipped"));
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
			UE_LOG(LogGPTerrainDeformationContract, Warning,
				TEXT("gp.Terrain.RunDeformationContractTest: missing world or client"));
			return;
		}
		if (GRunner.bActive)
		{
			UE_LOG(LogGPTerrainDeformationContract, Warning,
				TEXT("gp.Terrain.RunDeformationContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("TerrainDeformation"), TEXT("Terrain"), Token))
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
		TEXT("gp.Terrain.RunDeformationContractTest"),
		TEXT("Authority terrain deformation contracts: validation, shallow cap, sequence, duplicate guard."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));
}

#endif
