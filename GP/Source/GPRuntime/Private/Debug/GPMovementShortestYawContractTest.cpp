// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/GPMovementShortestYawContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AI/NavigationSystemBase.h"
#include "Buildings/GPMainBase.h"
#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/RecastNavMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPShortestYaw, Log, All);

namespace GPShortestYawDebug
{
	static TWeakObjectPtr<UGP_MovementShortestYawContractTestRunner> GActiveRunner;

	static void RunShortestYawContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPShortestYaw, Warning, TEXT("gp.Movement.RunShortestYawContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPShortestYaw, Warning, TEXT("gp.Movement.RunShortestYawContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("MovementShortestYawContract"), TEXT("MovementShortestYaw"), Token))
		{
			return;
		}

		UGP_MovementShortestYawContractTestRunner* Runner =
			NewObject<UGP_MovementShortestYawContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GShortestYawContract(
		TEXT("gp.Movement.RunShortestYawContractTest"),
		TEXT("GP-S41M shortest-yaw movement facing contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunShortestYawContractTest));

	static void RebuildNavAround(UWorld* World, const FVector& Center, float RadiusCm)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			return;
		}
		const FVector Extent(RadiusCm, RadiusCm, FMath::Max(RadiusCm, 400.0f));
		NavSys->AddDirtyArea(FBox(Center - Extent, Center + Extent), ENavigationDirtyFlag::All);
		NavSys->Build();
	}

	static UBoxComponent* AttachNavCarverBox(AActor* Owner, const FName& Name, const FVector& Extent)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(Owner, Name);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Box->SetCanEverAffectNavigation(true);
		Box->bDynamicObstacle = true;
		Box->SetAreaClassOverride(UNavArea_Null::StaticClass());
		Box->SetGenerateOverlapEvents(false);
		if (Owner->GetRootComponent() != nullptr)
		{
			Box->SetupAttachment(Owner->GetRootComponent());
		}
		else
		{
			Owner->SetRootComponent(Box);
		}
		Box->RegisterComponent();
		FNavigationSystem::UpdateComponentData(*Box);
		return Box;
	}
}

void UGP_MovementShortestYawContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_MovementShortestYawContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_MovementShortestYawContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		bCancelled = true;
		CancelReason = FName(TEXT("WorldCleanup"));
		Finish();
	}
}

void UGP_MovementShortestYawContractTestRunner::CleanupActors()
{
	if (AGP_SalvageWalker* Actor = WalkerWeak.Get())
	{
		Actor->Destroy();
	}
	WalkerWeak.Reset();
	if (AGP_Worker* Worker = WorkerWeak.Get())
	{
		Worker->Destroy();
	}
	WorkerWeak.Reset();
	if (AGP_MainBase* Building = BuildingWeak.Get())
	{
		Building->Destroy();
	}
	BuildingWeak.Reset();
	if (AActor* Carver = CarverWeak.Get())
	{
		Carver->Destroy();
	}
	CarverWeak.Reset();
}

void UGP_MovementShortestYawContractTestRunner::Finish()
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
	UE_LOG(LogGPShortestYaw, Log,
		TEXT("gp.Movement.RunShortestYawContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPShortestYawDebug::GActiveRunner.Reset();
}

void UGP_MovementShortestYawContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPShortestYaw, Error, TEXT("gp.Movement.RunShortestYawContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_MovementShortestYawContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPShortestYaw, Error, TEXT("gp.Movement.RunShortestYawContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPShortestYaw, Log, TEXT("gp.Movement.RunShortestYawContractTest PASS: %s"), Label);
	return true;
}

void UGP_MovementShortestYawContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_MovementShortestYawContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_MovementShortestYawContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_MovementShortestYawContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPShortestYaw, Log, TEXT("gp.Movement.RunShortestYawContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.05f);
}

void UGP_MovementShortestYawContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		constexpr float Step = 5.0f;

		const FRotator Legacy = FMath::RInterpConstantTo(
			FRotator(0.0f, 350.0f, 0.0f),
			FRotator(0.0f, 10.0f, 0.0f),
			1.0f / 60.0f,
			360.0f);
		const float LegacyApplied = Legacy.Yaw - 350.0f;
		Expect(LegacyApplied < -1.0f, TEXT("RootCause_RInterpConstantToChoosesLongYaw"));
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("GP-S41M root-cause repro: RInterpConstantTo(350->10, dt=1/60, speed=360) Applied=%.3f (long path)"),
			LegacyApplied);

		const float A = UGP_MovementComponent::ComputeShortestYawStep(350.0f, 10.0f, Step);
		const float ADelta = FMath::FindDeltaAngleDegrees(350.0f, A);
		Expect(ADelta > 0.0f && FMath::IsNearlyEqual(ADelta, Step, 0.01f), TEXT("A_350To10PositiveShort"));

		const float B = UGP_MovementComponent::ComputeShortestYawStep(10.0f, 350.0f, Step);
		const float BDelta = FMath::FindDeltaAngleDegrees(10.0f, B);
		Expect(BDelta < 0.0f && FMath::IsNearlyEqual(BDelta, -Step, 0.01f), TEXT("B_10To350NegativeShort"));

		const float C = UGP_MovementComponent::ComputeShortestYawStep(179.0f, -179.0f, 1.0f);
		const float CDelta = FMath::FindDeltaAngleDegrees(179.0f, C);
		Expect(CDelta > 0.0f && FMath::IsNearlyEqual(CDelta, 1.0f, 0.01f), TEXT("C_179ToNeg179PositiveCross"));
		Expect(FMath::IsNearlyEqual(
			UGP_MovementComponent::ComputeShortestYawStep(179.0f, -179.0f, 5.0f),
			FRotator::NormalizeAxis(-179.0f),
			0.01f),
			TEXT("C_179SnapsAcrossBoundary"));

		const float D = UGP_MovementComponent::ComputeShortestYawStep(-179.0f, 179.0f, 1.0f);
		const float DDelta = FMath::FindDeltaAngleDegrees(-179.0f, D);
		Expect(DDelta < 0.0f && FMath::IsNearlyEqual(DDelta, -1.0f, 0.01f), TEXT("D_Neg179To179NegativeCross"));

		const float E = UGP_MovementComponent::ComputeShortestYawStep(0.0f, 90.0f, 12.5f);
		Expect(FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, E)) <= 12.5f + KINDA_SMALL_NUMBER, TEXT("E_StepDoesNotExceedMax"));
		Expect(FMath::Abs(ADelta) <= Step + KINDA_SMALL_NUMBER, TEXT("E_ADoesNotExceedMax"));
		Expect(FMath::Abs(BDelta) <= Step + KINDA_SMALL_NUMBER, TEXT("E_BDoesNotExceedMax"));

		const float F = UGP_MovementComponent::ComputeShortestYawStep(5.0f, -5.0f, 15.0f);
		Expect(FMath::IsNearlyEqual(F, FRotator::NormalizeAxis(-5.0f), 0.01f), TEXT("F_SmallDeltaSnapsToTarget"));
		Expect(FMath::IsNearlyEqual(
			UGP_MovementComponent::ComputeShortestYawStep(-5.0f, 5.0f, 15.0f),
			FRotator::NormalizeAxis(5.0f),
			0.01f),
			TEXT("F_Neg5To5Snaps"));

		float Cursor = 350.0f;
		bool bConverged = false;
		for (int32 i = 0; i < 16; ++i)
		{
			Cursor = UGP_MovementComponent::ComputeShortestYawStep(Cursor, 10.0f, 20.0f);
			if (FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(Cursor, 10.0f), 0.0f, 0.01f))
			{
				bConverged = true;
				break;
			}
		}
		Expect(bConverged, TEXT("G_RepeatedStepsConverge"));

		const float H = UGP_MovementComponent::ComputeShortestYawStep(0.0f, 90.0f, 30.0f);
		Expect(FMath::IsNearlyEqual(H, 30.0f, 0.01f), TEXT("H_ZeroTo90StillWorks"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		const FVector Origin(-48000.0f, -16000.0f, 100.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_SalvageWalker* Walker = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(), Origin, FRotator(0.0f, 350.0f, 0.0f), Params);
		WalkerWeak = Walker;
		if (!Expect(IsValid(Walker), TEXT("I_SpawnWalker")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireScanIntervalSeconds = 10000.0f;
			Cmd->RefreshCombatAutoAcquireTimer();
		}

		UGP_MovementComponent* Movement = Walker->FindComponentByClass<UGP_MovementComponent>();
		if (!Expect(Movement != nullptr, TEXT("I_HasMovement")))
		{
			Finish();
			return;
		}

		Movement->SeparationStrength = 0.0f;
		Movement->RotationSpeed = 360.0f;
		Movement->bRotateToMovement = true;
		Walker->SetActorRotation(FRotator(0.0f, 350.0f, 0.0f));

		const float HeadingRad = FMath::DegreesToRadians(10.0f);
		const FVector Dest = Origin + FVector(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad), 0.0f) * 2500.0f;
		constexpr uint32 Serial = 41;
		const int32 PathBefore = Movement->GetActivePathPointCount();
		const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Dest, Serial);
		if (!Expect(Outcome.IsAccepted(), TEXT("I_MoveAccepted")))
		{
			Finish();
			return;
		}

		const float YawBefore = Walker->GetActorRotation().Yaw;
		const uint32 SerialBefore = Movement->GetActiveMoveSerial();
		const FVector DestBefore = Movement->GetMoveDestination();
		const int32 PathAfterAccept = Movement->GetActivePathPointCount();

		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);

		const float YawAfter = Walker->GetActorRotation().Yaw;
		const float Applied = FMath::FindDeltaAngleDegrees(YawBefore, YawAfter);
		Expect(Applied > 0.0f, TEXT("I_LiveFirstYawFollowsShortestPositive"));
		Expect(FMath::Abs(Applied) <= (360.0f / 60.0f) + 0.05f, TEXT("I_LiveStepWithinRotationSpeed"));
		Expect(Movement->GetActiveMoveSerial() == SerialBefore && SerialBefore == Serial, TEXT("I_SerialUnchanged"));
		Expect(Movement->GetMoveDestination().Equals(DestBefore, 0.1f), TEXT("I_DestinationUnchanged"));
		Expect(Movement->GetActivePathPointCount() == PathAfterAccept, TEXT("I_PathCountUnchanged"));
		Expect(PathAfterAccept >= PathBefore, TEXT("I_PathStillPresent"));
		Expect(Movement->IsMoving(), TEXT("I_StillMoving"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		if (AGP_SalvageWalker* Previous = WalkerWeak.Get())
		{
			Previous->Destroy();
			WalkerWeak.Reset();
		}

		const FVector ArenaSpawn(80.0f, -1400.0f, 88.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_SalvageWalker* Walker = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(), ArenaSpawn, FRotator(0.0f, 90.0f, 0.0f), Params);
		WalkerWeak = Walker;
		if (!Expect(IsValid(Walker), TEXT("J_SpawnOnArena")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireScanIntervalSeconds = 10000.0f;
			Cmd->RefreshCombatAutoAcquireTimer();
		}

		UGP_MovementComponent* Movement = Walker->FindComponentByClass<UGP_MovementComponent>();
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!Expect(Movement != nullptr && NavSys != nullptr, TEXT("J_HasMovementAndNavSys")))
		{
			Finish();
			return;
		}

		Movement->SeparationStrength = 0.0f;
		Movement->RotationSpeed = 360.0f;
		Movement->bRotateToMovement = true;

		const FVector Extent(
			Movement->NavProjectionExtentXY,
			Movement->NavProjectionExtentXY,
			Movement->NavProjectionExtentZ);
		FNavLocation SeedProjected;
		if (!Expect(NavSys->ProjectPointToNavigation(ArenaSpawn, SeedProjected, Extent),
			TEXT("J_SeedProjectsToNav")))
		{
			Finish();
			return;
		}

		Walker->SetActorLocation(FVector(SeedProjected.Location.X, SeedProjected.Location.Y, 88.0f));
		Walker->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		GPShortestYawDebug::RebuildNavAround(World, Walker->GetActorLocation(), 600.0f);

		const FVector ActualStart = Walker->GetActorLocation();
		FVector Dest = ActualStart + FVector(1200.0f, 0.0f, 0.0f);
		Dest.X = FMath::Clamp(Dest.X, -1800.0f, 1800.0f);
		Dest.Y = FMath::Clamp(Dest.Y, -1800.0f, 1800.0f);
		if (FVector::Dist2D(ActualStart, Dest) < 400.0f)
		{
			Dest = FVector(0.0f, 0.0f, ActualStart.Z);
		}
		const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Dest, 42);
		if (!Expect(Outcome.IsAccepted() && Movement->IsActivePathFromNavigation(), TEXT("J_FirstMoveUsedNav")))
		{
			Finish();
			return;
		}

		const FVector ProjectedStart = Movement->DebugGetLastProjectedStart();
		const float DistActualProjected = FVector::Dist2D(ActualStart, ProjectedStart);
		FVector Path0 = FVector::ZeroVector;
		FVector Path1 = FVector::ZeroVector;
		const bool bHas0 = Movement->TryGetActivePathPoint(0, Path0);
		const bool bHas1 = Movement->TryGetActivePathPoint(1, Path1);
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("J_FirstMoveDiag ActualStart=%s ProjectedStart=%s DistActualProjected=%.1f PathIndex=%d PathPoints=%d Path0=%s Path1=%s Yaw=%.2f"),
			*ActualStart.ToCompactString(),
			*ProjectedStart.ToCompactString(),
			DistActualProjected,
			Movement->GetActivePathIndex(),
			Movement->GetActivePathPointCount(),
			bHas0 ? *Path0.ToCompactString() : TEXT("none"),
			bHas1 ? *Path1.ToCompactString() : TEXT("none"),
			Walker->GetActorRotation().Yaw);

		Expect(!Walker->HasAnyPrimitiveThatCanAffectNavigation(), TEXT("J_WalkerNotNavRelevant"));
		Expect(DistActualProjected <= FMath::Max(Movement->AcceptanceRadius, 25.0f),
			TEXT("J_FirstMoveProjectedStartNearActual"));
		Expect(bHas0, TEXT("J_HasPath0"));

		const FVector2D ToDest(Dest.X - ActualStart.X, Dest.Y - ActualStart.Y);
		const float YawBefore = Walker->GetActorRotation().Yaw;
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		const FVector After = Walker->GetActorLocation();
		const FVector2D Moved(After.X - ActualStart.X, After.Y - ActualStart.Y);
		const float YawAfter = Walker->GetActorRotation().Yaw;
		const float Applied = FMath::FindDeltaAngleDegrees(YawBefore, YawAfter);
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("J_FirstTick Moved=%s YawBefore=%.2f YawAfter=%.2f Applied=%.2f"),
			*FVector(Moved.X, Moved.Y, 0.0f).ToCompactString(),
			YawBefore,
			YawAfter,
			Applied);

		Expect(FVector2D::DotProduct(Moved, ToDest) > 0.0f, TEXT("J_FirstStepTowardCommandedDest"));
		Expect(FVector::Dist2D(After, ActualStart) < 80.0f, TEXT("J_FirstStepNotNinetyCmSideways"));
		const float DestYaw = FMath::RadiansToDegrees(FMath::Atan2(ToDest.Y, ToDest.X));
		const float ExpectedDelta = FMath::FindDeltaAngleDegrees(YawBefore, DestYaw);
		Expect(Applied * ExpectedDelta > 0.0f, TEXT("J_FirstYawShortestTowardMoveDir"));
		Expect(FMath::Abs(Applied) <= (360.0f / 60.0f) + 0.05f, TEXT("J_FirstYawWithinRotationSpeed"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		if (AGP_SalvageWalker* Previous = WalkerWeak.Get())
		{
			Previous->Destroy();
			WalkerWeak.Reset();
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!Expect(NavSys != nullptr, TEXT("K_HasNavSys")))
		{
			Finish();
			return;
		}

		const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData);
		const ERuntimeGenerationType GenMode = Recast != nullptr
			? Recast->GetRuntimeGenerationMode()
			: ERuntimeGenerationType::Static;
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("K_NavRuntimeGeneration=%d SupportsRuntimeGeneration=%s"),
			static_cast<int32>(GenMode),
			(NavData != nullptr && NavData->SupportsRuntimeGeneration()) ? TEXT("true") : TEXT("false"));

		const FVector ProbeGuess(80.0f, -1400.0f, 88.0f);
		const FVector Extent(250.0f, 250.0f, 400.0f);
		FNavLocation Seed;
		if (!Expect(NavSys->ProjectPointToNavigation(ProbeGuess, Seed, Extent), TEXT("K_SeedOnNav")))
		{
			Finish();
			return;
		}
		NavProbeLocation = FVector(Seed.Location.X, Seed.Location.Y, 88.0f);
		const float DistSeed = FVector::Dist2D(NavProbeLocation, Seed.Location);
		Expect(DistSeed <= 25.0f, TEXT("K_EmptyProbeOnNav"));

		FNavLocation OffNav;
		const bool bOffProjects = NavSys->ProjectPointToNavigation(
			FVector(-8000.0f, -8000.0f, 88.0f), OffNav, FVector(50.0f, 50.0f, 100.0f));
		const float DistOff = bOffProjects ? FVector::Dist2D(FVector(-8000.0f, -8000.0f, 88.0f), OffNav.Location) : 1000.0f;
		UE_LOG(LogGPShortestYaw, Log, TEXT("K_OffNavProbe Projects=%s Dist=%.1f"), bOffProjects ? TEXT("true") : TEXT("false"), DistOff);
		Expect(!bOffProjects || DistOff > 50.0f, TEXT("K_ProjectionDetectsMissingNav"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const FVector Extent(250.0f, 250.0f, 400.0f);
		FNavLocation Restored;
		if (!Expect(NavSys != nullptr
			&& NavSys->ProjectPointToNavigation(NavProbeLocation, Restored, Extent)
			&& FVector::Dist2D(NavProbeLocation, Restored.Location) <= 25.0f,
			TEXT("K_ProbeStillOnNavBeforeUnits")))
		{
			Finish();
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_SalvageWalker* Walker = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(), NavProbeLocation, FRotator::ZeroRotator, Params);
		WalkerWeak = Walker;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), NavProbeLocation + FVector(250.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
		WorkerWeak = Worker;
		if (!Expect(IsValid(Walker) && IsValid(Worker), TEXT("K_SpawnMobileUnits")))
		{
			Finish();
			return;
		}

		UBoxComponent* WalkerFake = GPShortestYawDebug::AttachNavCarverBox(
			Walker, TEXT("SimulatedAuthoredMesh"), FVector(140.0f, 140.0f, 120.0f));
		UBoxComponent* WorkerFake = GPShortestYawDebug::AttachNavCarverBox(
			Worker, TEXT("SimulatedAuthoredMesh"), FVector(140.0f, 140.0f, 120.0f));
		Walker->ApplyMobileNavigationGenerationPolicy();
		Worker->ApplyMobileNavigationGenerationPolicy();
		Expect(WalkerFake != nullptr && !WalkerFake->CanEverAffectNavigation(), TEXT("K_WalkerAuthoredMeshForcedOff"));
		Expect(WorkerFake != nullptr && !WorkerFake->CanEverAffectNavigation(), TEXT("K_WorkerAuthoredMeshForcedOff"));
		Expect(!Walker->HasAnyPrimitiveThatCanAffectNavigation(), TEXT("K_WalkerNoNavPrimitives"));
		Expect(!Worker->HasAnyPrimitiveThatCanAffectNavigation(), TEXT("K_WorkerNoNavPrimitives"));

		AGP_MainBase* Building = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			NavProbeLocation + FVector(0.0f, 900.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		BuildingWeak = Building;
		if (Expect(IsValid(Building) && Building->GetNavigationObstacle() != nullptr, TEXT("K_SpawnBuilding")))
		{
			Expect(Building->GetNavigationObstacle()->CanEverAffectNavigation(),
				TEXT("K_BuildingObstacleStillNavRelevant"));
		}

		GPShortestYawDebug::RebuildNavAround(World, NavProbeLocation, 800.0f);
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 5:
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const FVector Extent(250.0f, 250.0f, 400.0f);
		FNavLocation AfterMobile;
		const bool bStillOnNav = NavSys != nullptr
			&& NavSys->ProjectPointToNavigation(NavProbeLocation, AfterMobile, Extent);
		const float DistAfterMobile = bStillOnNav
			? FVector::Dist2D(NavProbeLocation, AfterMobile.Location)
			: 1000.0f;
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("K_AfterMobilePlace Dist=%.1f Projected=%s"),
			DistAfterMobile,
			bStillOnNav ? *AfterMobile.Location.ToCompactString() : TEXT("none"));
		Expect(bStillOnNav && DistAfterMobile <= 25.0f, TEXT("K_MobileUnitDoesNotCarveOwnXY"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_MovementShortestYawContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}

void UGP_MovementShortestYawContractTestRunner::Start(UWorld* InWorld)
{
	(void)InWorld;
}

void UGP_MovementShortestYawContractTestRunner::ScheduleNext(float DelaySeconds)
{
	(void)DelaySeconds;
}

void UGP_MovementShortestYawContractTestRunner::AdvanceStage() {}

bool UGP_MovementShortestYawContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}

void UGP_MovementShortestYawContractTestRunner::Abort(const TCHAR* Reason)
{
	(void)Reason;
}

void UGP_MovementShortestYawContractTestRunner::Finish()
{
	bFinished = true;
}

void UGP_MovementShortestYawContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}

void UGP_MovementShortestYawContractTestRunner::UnbindWorldCleanup() {}

void UGP_MovementShortestYawContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
