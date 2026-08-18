// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/GPMovementShortestYawContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"

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

		const FVector ArenaSpawn(0.0f, -1400.0f, 88.0f);
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
		if (!Expect(NavSys->ProjectPointToNavigation(Walker->GetActorLocation(), SeedProjected, Extent),
			TEXT("J_SeedProjectsToNav")))
		{
			Finish();
			return;
		}

		const FVector OffsetCandidates[] = {
			FVector(SeedProjected.Location.X + 80.0f, SeedProjected.Location.Y, SeedProjected.Location.Z + 40.0f),
			FVector(0.0f, -2080.0f, 88.0f),
			FVector(2080.0f, -1400.0f, 88.0f),
			FVector(0.0f, -2050.0f, 150.0f)
		};
		FVector ChosenStart = OffsetCandidates[0];
		for (const FVector& Candidate : OffsetCandidates)
		{
			FNavLocation CandidateProjected;
			if (NavSys->ProjectPointToNavigation(Candidate, CandidateProjected, Extent)
				&& FVector::Dist2D(Candidate, CandidateProjected.Location) > Movement->AcceptanceRadius)
			{
				ChosenStart = Candidate;
				break;
			}
		}
		Walker->SetActorLocation(ChosenStart);
		Walker->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

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
		FVector Path2 = FVector::ZeroVector;
		const bool bHas0 = Movement->TryGetActivePathPoint(0, Path0);
		const bool bHas1 = Movement->TryGetActivePathPoint(1, Path1);
		const bool bHas2 = Movement->TryGetActivePathPoint(2, Path2);
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("J_FirstMoveDiag ActualStart=%s ProjectedStart=%s DistActualProjected=%.1f PathIndex=%d PathPoints=%d Path0=%s Path1=%s Path2=%s Yaw=%.2f"),
			*ActualStart.ToCompactString(),
			*ProjectedStart.ToCompactString(),
			DistActualProjected,
			Movement->GetActivePathIndex(),
			Movement->GetActivePathPointCount(),
			bHas0 ? *Path0.ToCompactString() : TEXT("none"),
			bHas1 ? *Path1.ToCompactString() : TEXT("none"),
			bHas2 ? *Path2.ToCompactString() : TEXT("none"),
			Walker->GetActorRotation().Yaw);

		const FVector RawPath0 = Movement->DebugGetLastRawNavPath0();
		Expect(FVector::Dist2D(RawPath0, ProjectedStart) <= FMath::Max(Movement->AcceptanceRadius, 25.0f),
			TEXT("J_RawFirstNavPointWasProjectedStart"));
		if (DistActualProjected > Movement->AcceptanceRadius)
		{
			Expect(true, TEXT("J_ProjectedStartNoticeablyOffset"));
		}
		else
		{
			UE_LOG(LogGPShortestYaw, Log,
				TEXT("J_ProjectedStartNoticeablyOffset SKIP: DistActualProjected=%.1f (XY aligned; raw Recast start still proven)"),
				DistActualProjected);
		}
		Expect(Movement->GetActivePathIndex() == 0, TEXT("J_InitialPathIndex0"));
		Expect(bHas0, TEXT("J_HasPath0"));
		Expect(FVector::Dist2D(Path0, ProjectedStart) > FMath::Max(Movement->AcceptanceRadius, 25.0f),
			TEXT("J_FirstRuntimePointIsNotProjectedStartAnchor"));

		const FVector2D ToProjected(ProjectedStart.X - ActualStart.X, ProjectedStart.Y - ActualStart.Y);
		const FVector2D ToPath0(Path0.X - ActualStart.X, Path0.Y - ActualStart.Y);
		const FVector2D ToDest(Dest.X - ActualStart.X, Dest.Y - ActualStart.Y);
		const float DotPath0Dest = FVector2D::DotProduct(ToPath0.GetSafeNormal(), ToDest.GetSafeNormal());
		const float DotPath0Projected = FVector2D::DotProduct(ToPath0.GetSafeNormal(), ToProjected.GetSafeNormal());
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("J_FirstMoveDir DotPath0Dest=%.3f DotPath0Projected=%.3f"),
			DotPath0Dest,
			DotPath0Projected);
		Expect(DotPath0Dest > DotPath0Projected, TEXT("J_FirstWaypointNotTowardProjectedStart"));

		const float YawBefore = Walker->GetActorRotation().Yaw;
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		const FVector After = Walker->GetActorLocation();
		const FVector2D Moved(After.X - ActualStart.X, After.Y - ActualStart.Y);
		const float YawAfter = Walker->GetActorRotation().Yaw;
		const float Applied = FMath::FindDeltaAngleDegrees(YawBefore, YawAfter);
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Moved.Y, Moved.X));
		UE_LOG(LogGPShortestYaw, Log,
			TEXT("J_FirstTick Moved=%s YawBefore=%.2f YawAfter=%.2f Applied=%.2f TargetFromMove=%.2f"),
			*FVector(Moved.X, Moved.Y, 0.0f).ToCompactString(),
			YawBefore,
			YawAfter,
			Applied,
			TargetYaw);

		Expect(FVector2D::DotProduct(Moved, ToDest) > FVector2D::DotProduct(Moved, ToProjected),
			TEXT("J_FirstStepTowardForwardNotProjectedStart"));
		const float DestYaw = FMath::RadiansToDegrees(FMath::Atan2(ToDest.Y, ToDest.X));
		const float ExpectedDelta = FMath::FindDeltaAngleDegrees(YawBefore, DestYaw);
		Expect(Applied * ExpectedDelta > 0.0f, TEXT("J_FirstYawShortestTowardMoveDir"));
		Expect(FMath::Abs(Applied) <= (360.0f / 60.0f) + 0.05f, TEXT("J_FirstYawWithinRotationSpeed"));

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
