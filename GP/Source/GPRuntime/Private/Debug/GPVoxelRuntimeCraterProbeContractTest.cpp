// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/GPVoxelRuntimeCraterProbeContractTest.h"

#if !UE_BUILD_SHIPPING

#include "CollisionQueryParams.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVoxelRuntimeCraterProbe, Log, All);

namespace GPVoxelRuntimeCraterProbeDebug
{
	static TWeakObjectPtr<UGP_VoxelRuntimeCraterProbeContractTestRunner> GActiveRunner;

	constexpr float ProbeOriginX = 25000.f;
	constexpr float ProbeOriginY = 0.f;
	constexpr float ProbeOriginZ = 200.f;
	constexpr float CraterRadiusCm = 300.f;
	constexpr int32 FarVoxelOffset = 20;
	constexpr int32 MaxReadyTicks = 200;
	constexpr int32 MaxMeshIdleTicks = 120;
	constexpr int32 MaxCollisionTicks = 120;
	constexpr float TickSeconds = 0.05f;
	constexpr float CollisionDeepenCm = 120.f;

	static void RunRuntimeCraterProbeContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPVoxelRuntimeCraterProbe, Warning,
				TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPVoxelRuntimeCraterProbe, Warning,
				TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("VoxelRuntimeCraterProbe"), TEXT("Voxel"), Token))
		{
			return;
		}

		UGP_VoxelRuntimeCraterProbeContractTestRunner* Runner =
			NewObject<UGP_VoxelRuntimeCraterProbeContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GRuntimeCraterProbeContract(
		TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest"),
		TEXT("Spawn a tiny UVoxelFlatGenerator AVoxelWorld and prove RemoveSphere crater + collision."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRuntimeCraterProbeContractTest));
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_VoxelRuntimeCraterProbeContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
		TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest Start"));
	AdvanceStage();
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldOnSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_VoxelRuntimeCraterProbeContractTestRunner::AdvanceStage),
		FMath::Max(0.01f, DelaySeconds),
		false);
}

bool UGP_VoxelRuntimeCraterProbeContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Error,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
		TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest PASS: %s"), Label);
	return true;
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPVoxelRuntimeCraterProbe, Error,
		TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
		TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPVoxelRuntimeCraterProbeDebug::GActiveRunner.Reset();
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == WorldWeak.Get())
	{
		bCancelled = true;
		CancelReason = TEXT("WorldCleanup");
		Finish();
	}
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::CleanupActors()
{
	if (AActor* Probe = ProbeWeak.Get())
	{
		GPVoxelRuntimeProbeAdapter::DestroyProbeWorld(Probe);
	}
	ProbeWeak.Reset();
}

void UGP_VoxelRuntimeCraterProbeContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	AActor* Probe = ProbeWeak.Get();

	switch (StageIndex)
	{
	case 0:
	{
		ProbeOrigin = FVector(
			GPVoxelRuntimeCraterProbeDebug::ProbeOriginX,
			GPVoxelRuntimeCraterProbeDebug::ProbeOriginY,
			GPVoxelRuntimeCraterProbeDebug::ProbeOriginZ);
		Probe = GPVoxelRuntimeProbeAdapter::SpawnConfiguredProbeWorld(World, ProbeOrigin);
		if (!Expect(Probe != nullptr, TEXT("A_SpawnedVoxelWorld")))
		{
			Finish();
			return;
		}
		ProbeWeak = Probe;
		if (!Expect(GPVoxelRuntimeProbeAdapter::CreateWorldIfNeeded(Probe), TEXT("B_CreateWorld")))
		{
			Finish();
			return;
		}
		Expect(GPVoxelRuntimeProbeAdapter::IsCreated(Probe), TEXT("C_IsCreated"));
		Expect(FMath::IsNearlyEqual(GPVoxelRuntimeProbeAdapter::GetVoxelSizeCm(Probe), 100.f),
			TEXT("D_VoxelSize100cm"));
		++StageIndex;
		ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
		break;
	}
	case 1:
	{
		++ReadyWaitTicks;
		const bool bReady = GPVoxelRuntimeProbeAdapter::IsCreated(Probe)
			&& GPVoxelRuntimeProbeAdapter::CountProcMeshComponents(Probe) > 0
			&& GPVoxelRuntimeProbeAdapter::IsMeshIdle(Probe);
		if (!bReady)
		{
			if (ReadyWaitTicks >= GPVoxelRuntimeCraterProbeDebug::MaxReadyTicks)
			{
				Abort(TEXT("ReadyTimeout"));
				return;
			}
			ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
			return;
		}
		Expect(true, TEXT("E_WorldReady"));
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: ready IsLoaded=%s MeshIdle=%s Tasks=%d Meshes=%d WaitTicks=%d"),
			GPVoxelRuntimeProbeAdapter::IsLoaded(Probe) ? TEXT("true") : TEXT("false"),
			GPVoxelRuntimeProbeAdapter::IsMeshIdle(Probe) ? TEXT("true") : TEXT("false"),
			GPVoxelRuntimeProbeAdapter::GetMeshTaskCount(Probe),
			GPVoxelRuntimeProbeAdapter::CountProcMeshComponents(Probe),
			ReadyWaitTicks);
		++StageIndex;
		ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
		break;
	}
	case 2:
	{
		if (!GPVoxelRuntimeProbeAdapter::IsMeshIdle(Probe))
		{
			++MeshIdleWaitTicks;
			if (MeshIdleWaitTicks >= GPVoxelRuntimeCraterProbeDebug::MaxMeshIdleTicks)
			{
				Abort(TEXT("InitialMeshIdleTimeout"));
				return;
			}
			ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
			return;
		}

		CraterVoxel = FIntVector(0, 0, -1);
		FarVoxel = FIntVector(GPVoxelRuntimeCraterProbeDebug::FarVoxelOffset, 0, -1);
		CraterWorld = GPVoxelRuntimeProbeAdapter::VoxelToWorld(Probe, CraterVoxel);
		FarWorld = GPVoxelRuntimeProbeAdapter::VoxelToWorld(Probe, FarVoxel);
		const FIntVector RoundTrip = GPVoxelRuntimeProbeAdapter::WorldToVoxel(Probe, CraterWorld);
		Expect(RoundTrip == CraterVoxel, TEXT("F_GlobalToLocalRoundTrip"));

		if (!Expect(GPVoxelRuntimeProbeAdapter::QueryDensityAtVoxel(Probe, CraterVoxel, BaselineCraterDensity),
			TEXT("G_BaselineCraterDensityQuery")))
		{
			Finish();
			return;
		}
		Expect(GPVoxelRuntimeProbeAdapter::QueryDensityAtVoxel(Probe, FarVoxel, BaselineFarDensity),
			TEXT("H_BaselineFarDensityQuery"));
		Expect(BaselineCraterDensity < 0.f, TEXT("I_BaselineCraterSolid"));
		Expect(BaselineFarDensity < 0.f, TEXT("J_BaselineFarSolid"));

		float SurfaceZ = 0.f;
		Expect(GPVoxelRuntimeProbeAdapter::QueryDensitySurfaceWorldZ(Probe, ProbeOrigin.X, ProbeOrigin.Y, SurfaceZ),
			TEXT("K_DensitySurfaceZ"));
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: DensitySurfaceZ=%.1f ActorZ=%.1f"),
			SurfaceZ, ProbeOrigin.Z);

		bool bClosest = false;
		FIntVector ClosestVoxel = FIntVector::ZeroValue;
		float ClosestDensity = 0.f;
		GPVoxelRuntimeProbeAdapter::QueryClosestNonEmpty(
			Probe, CraterWorld, bClosest, ClosestVoxel, ClosestDensity);
		Expect(bClosest, TEXT("L_FindClosestNonEmptyInsideSolid"));
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: FindClosestNonEmpty only searches voxel neighbors of the query point (not XY-to-Z). success=%s voxel=(%d,%d,%d) density=%.3f"),
			bClosest ? TEXT("true") : TEXT("false"),
			ClosestVoxel.X, ClosestVoxel.Y, ClosestVoxel.Z, ClosestDensity);

		FHitResult CraterHit;
		const FVector CraterTraceStart = CraterWorld + FVector(0.f, 0.f, 800.f);
		const FVector CraterTraceEnd = CraterWorld - FVector(0.f, 0.f, 1200.f);
		const bool bCraterHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(
			World, Probe, CraterTraceStart, CraterTraceEnd, CraterHit);
		Expect(bCraterHit, TEXT("M_BaselineCraterTraceHit"));
		BaselineCraterHitZ = CraterHit.ImpactPoint.Z;

		FHitResult FarHit;
		const FVector FarTraceStart = FarWorld + FVector(0.f, 0.f, 800.f);
		const FVector FarTraceEnd = FarWorld - FVector(0.f, 0.f, 1200.f);
		const bool bFarHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(
			World, Probe, FarTraceStart, FarTraceEnd, FarHit);
		Expect(bFarHit, TEXT("N_BaselineFarTraceHit"));
		BaselineFarHitZ = FarHit.ImpactPoint.Z;
		BaselineMeshCount = GPVoxelRuntimeProbeAdapter::CountProcMeshComponents(Probe);
		Expect(BaselineMeshCount > 0, TEXT("M2_ProcMeshGenerated"));

		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: baseline craterDensity=%.3f farDensity=%.3f craterHitZ=%.1f farHitZ=%.1f meshes=%d"),
			BaselineCraterDensity, BaselineFarDensity, BaselineCraterHitZ, BaselineFarHitZ, BaselineMeshCount);

		++StageIndex;
		AdvanceStage();
		break;
	}
	case 3:
	{
		FGPVoxelSphereSubtractRequest Request;
		Request.WorldLocation = ProbeOrigin;
		Request.RadiusCm = GPVoxelRuntimeCraterProbeDebug::CraterRadiusCm;
		FGPVoxelIntBoxReport Edited;
		const bool bApplied = GPVoxelRuntimeProbeAdapter::ApplySphereSubtract(Probe, Request, Edited);
		Expect(bApplied, TEXT("O_RemoveSphereReturnedBounds"));
		Expect(Edited.bValid, TEXT("P_EditedBoundsValid"));
		Expect(!Edited.bInfinite, TEXT("Q_EditedBoundsNotInfinite"));

		const FIntVector EditedSize = Edited.Max - Edited.Min;
		const FIntVector WorldSize = GPVoxelRuntimeProbeAdapter::GetWorldBoundsSize(Probe);
		Expect(EditedSize.GetMax() > 0, TEXT("R_EditedBoundsNonEmpty"));
		Expect(EditedSize.X < WorldSize.X && EditedSize.Y < WorldSize.Y && EditedSize.Z < WorldSize.Z,
			TEXT("S_EditedBoundsLocalVsWorld"));
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: RemoveSphere Position=world cm Radius=%.1fcm bMultiThreaded=false bConvertToVoxelSpace=true bUpdateRender=true EditedBounds=(%d/%d, %d/%d, %d/%d) Size=(%d,%d,%d) WorldSize=(%d,%d,%d)"),
			Request.RadiusCm,
			Edited.Min.X, Edited.Max.X, Edited.Min.Y, Edited.Max.Y, Edited.Min.Z, Edited.Max.Z,
			EditedSize.X, EditedSize.Y, EditedSize.Z,
			WorldSize.X, WorldSize.Y, WorldSize.Z);

		Expect(GPVoxelRuntimeProbeAdapter::QueryDensityAtVoxel(Probe, CraterVoxel, AfterCraterDensity),
			TEXT("T_AfterCraterDensityQuery"));
		Expect(GPVoxelRuntimeProbeAdapter::QueryDensityAtVoxel(Probe, FarVoxel, AfterFarDensity),
			TEXT("U_AfterFarDensityQuery"));
		Expect(AfterCraterDensity > 0.f, TEXT("V_CraterCenterNowEmpty"));
		Expect(AfterFarDensity < 0.f, TEXT("W_FarStillSolid"));
		Expect(AfterCraterDensity > BaselineCraterDensity, TEXT("X_CraterDensityChanged"));
		Expect(FMath::IsNearlyEqual(AfterFarDensity, BaselineFarDensity, 0.05f), TEXT("Y_FarDensityUnchanged"));

		MeshIdleWaitTicks = 0;
		++StageIndex;
		ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
		break;
	}
	case 4:
	{
		++MeshIdleWaitTicks;
		if (!GPVoxelRuntimeProbeAdapter::IsMeshIdle(Probe))
		{
			if (MeshIdleWaitTicks >= GPVoxelRuntimeCraterProbeDebug::MaxMeshIdleTicks)
			{
				Abort(TEXT("PostEditMeshIdleTimeout"));
				return;
			}
			ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
			return;
		}
		Expect(true, TEXT("Z_PostEditMeshIdle"));
		AfterMeshCount = GPVoxelRuntimeProbeAdapter::CountProcMeshComponents(Probe);
		Expect(AfterMeshCount > 0, TEXT("AA_ProcMeshPresent"));
		UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
			TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: post-edit meshes=%d (baseline=%d) Tasks=%d"),
			AfterMeshCount, BaselineMeshCount, GPVoxelRuntimeProbeAdapter::GetMeshTaskCount(Probe));
		CollisionWaitTicks = 0;
		++StageIndex;
		ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
		break;
	}
	case 5:
	{
		FHitResult CraterHit;
		const FVector CraterTraceStart = CraterWorld + FVector(0.f, 0.f, 800.f);
		const FVector CraterTraceEnd = CraterWorld - FVector(0.f, 0.f, 1200.f);
		const bool bCraterHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(
			World, Probe, CraterTraceStart, CraterTraceEnd, CraterHit);
		const float AfterHitZ = bCraterHit ? CraterHit.ImpactPoint.Z : BaselineCraterHitZ;
		const bool bDeeper = bCraterHit && (AfterHitZ < BaselineCraterHitZ - GPVoxelRuntimeCraterProbeDebug::CollisionDeepenCm);

		if (!bDeeper)
		{
			++CollisionWaitTicks;
			if (CollisionWaitTicks < GPVoxelRuntimeCraterProbeDebug::MaxCollisionTicks)
			{
				ScheduleNext(GPVoxelRuntimeCraterProbeDebug::TickSeconds);
				return;
			}
			UE_LOG(LogGPVoxelRuntimeCraterProbe, Error,
				TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest FAIL: AB_CollisionDeepened baselineZ=%.1f afterHit=%s afterZ=%.1f waitTicks=%d"),
				BaselineCraterHitZ,
				bCraterHit ? TEXT("true") : TEXT("false"),
				AfterHitZ,
				CollisionWaitTicks);
			++Failures;
		}
		else
		{
			Expect(true, TEXT("AB_CollisionDeepened"));
			UE_LOG(LogGPVoxelRuntimeCraterProbe, Log,
				TEXT("gp.Voxel.RunRuntimeCraterProbeContractTest NOTE: collision baselineZ=%.1f afterZ=%.1f delta=%.1f waitTicks=%d"),
				BaselineCraterHitZ, AfterHitZ, BaselineCraterHitZ - AfterHitZ, CollisionWaitTicks);
		}

		FHitResult FarHit;
		const FVector FarTraceStart = FarWorld + FVector(0.f, 0.f, 800.f);
		const FVector FarTraceEnd = FarWorld - FVector(0.f, 0.f, 1200.f);
		const bool bFarHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(
			World, Probe, FarTraceStart, FarTraceEnd, FarHit);
		Expect(bFarHit, TEXT("AC_FarTraceStillHits"));
		if (bFarHit)
		{
			Expect(FMath::Abs(FarHit.ImpactPoint.Z - BaselineFarHitZ) < 80.f, TEXT("AD_FarTraceZUnchanged"));
		}

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_VoxelRuntimeCraterProbeContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_VoxelRuntimeCraterProbeContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_VoxelRuntimeCraterProbeContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_VoxelRuntimeCraterProbeContractTestRunner::AdvanceStage() {}
bool UGP_VoxelRuntimeCraterProbeContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_VoxelRuntimeCraterProbeContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_VoxelRuntimeCraterProbeContractTestRunner::Finish() { bFinished = true; }
void UGP_VoxelRuntimeCraterProbeContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_VoxelRuntimeCraterProbeContractTestRunner::UnbindWorldCleanup() {}
void UGP_VoxelRuntimeCraterProbeContractTestRunner::CleanupActors() {}

#endif
