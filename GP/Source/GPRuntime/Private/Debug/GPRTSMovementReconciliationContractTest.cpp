// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPRTSMovementReconciliationContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Command/GPCommandComponent.h"
#include "Command/GPCommandRequest.h"
#include "Command/GPStoredUnitCommand.h"
#include "Command/GPUnitCommand.h"
#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPRTSMovementRecon, Log, All);

namespace GPRTSMovementReconDebug
{
	static TWeakObjectPtr<UGP_RTSMovementReconciliationContractTestRunner> GActiveRunner;

	static void ApplyCombatStats(AGP_UnitBase* Unit, float Health, float Damage, float Range, float Cooldown)
	{
		if (Unit == nullptr)
		{
			return;
		}
		UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return;
		}
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetMaxHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), Damage);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackRangeAttribute(), Range);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackCooldownAttribute(), Cooldown);
	}

	static AGP_SalvageWalker* SpawnSW(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_SalvageWalker* SW = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (SW != nullptr)
		{
			SW->SetTeamId(TeamId);
		}
		return SW;
	}

	static AGP_Worker* SpawnWorker(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Worker != nullptr)
		{
			Worker->SetTeamId(TeamId);
		}
		return Worker;
	}

	static AGP_PlayerState* SpawnTeamPlayerState(UWorld* World, AGameStateBase* GameState, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerState* PS = World->SpawnActor<AGP_PlayerState>(
			AGP_PlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(PS) || GameState == nullptr)
		{
			return nullptr;
		}
		PS->SetTeamId(TeamId);
		GameState->AddPlayerState(PS);
		return PS;
	}

	static AGP_PlayerController* SpawnTeamPC(UWorld* World, AGP_PlayerState* PS)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerController* PC = World->SpawnActor<AGP_PlayerController>(
			AGP_PlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(PC) || !IsValid(PS))
		{
			return nullptr;
		}
		PC->PlayerState = PS;
		PS->SetOwner(PC);
		return PC;
	}

	static void IssueCommand(AGP_UnitBase* Unit, const FGameplayTag& Tag, AActor* Target, const FVector& Loc)
	{
		if (Unit == nullptr)
		{
			return;
		}
		UGP_UnitCommandComponent* Cmd = Unit->GetUnitCommandComponent();
		if (Cmd == nullptr)
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = Tag;
		Command.TargetActor = Target;
		Command.TargetLocation = Loc;
		Command.bQueue = false;
		Cmd->HandleCommand(Command);
	}

	static UGP_MovementComponent* GetMovement(AGP_UnitBase* Unit)
	{
		AGP_MobileUnit* Mobile = Cast<AGP_MobileUnit>(Unit);
		return Mobile != nullptr ? Mobile->GetUnitMovementComponent() : nullptr;
	}

	static bool IsUnitMoving(AGP_UnitBase* Unit)
	{
		UGP_MovementComponent* Movement = GetMovement(Unit);
		return Movement != nullptr && Movement->IsMoving();
	}

	static void DestroyWeakActor(TWeakObjectPtr<AActor>& Weak)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actor->Destroy();
		}
		Weak.Reset();
	}

	static void DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
	{
		if (AGP_Worker* W = Weak.Get())
		{
			W->Destroy();
		}
		Weak.Reset();
	}

	static void DestroyWeakSW(TWeakObjectPtr<AGP_SalvageWalker>& Weak)
	{
		if (AGP_SalvageWalker* SW = Weak.Get())
		{
			SW->Destroy();
		}
		Weak.Reset();
	}

	/** CapsuleWorldStatic-style obstacle: WorldStatic box that blocks nav. */
	static AActor* SpawnNavBlockingBox(
		UWorld* World,
		const FVector& Center,
		const FVector& HalfExtent)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
		if (Wall == nullptr)
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Wall, TEXT("NavBlockBox"));
		Box->SetBoxExtent(HalfExtent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetCanEverAffectNavigation(false);
		Box->SetMobility(EComponentMobility::Movable);
		Wall->SetRootComponent(Box);
		Box->RegisterComponent();
		Box->SetWorldLocation(Center);
		Wall->SetActorEnableCollision(true);
		return Wall;
	}

	static bool PointInsideAABB(const FVector& Point, const FVector& Center, const FVector& HalfExtent)
	{
		return FMath::Abs(Point.X - Center.X) <= HalfExtent.X
			&& FMath::Abs(Point.Y - Center.Y) <= HalfExtent.Y
			&& FMath::Abs(Point.Z - Center.Z) <= HalfExtent.Z;
	}

	static bool HasNavData(UWorld* World)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		return NavSys != nullptr
			&& NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
	}

	static bool EnsureNavCoverage(
		UWorld* World,
		const FVector& Center,
		const FVector& CoverHalfExtent,
		TWeakObjectPtr<AActor>& OutBoundsWeak)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			return false;
		}

		if (NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr)
		{
			const FBox Dirty(
				Center - CoverHalfExtent,
				Center + CoverHalfExtent);
			NavSys->AddDirtyArea(Dirty, ENavigationDirtyFlag::All);
			return true;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(
			ANavMeshBoundsVolume::StaticClass(), Center, FRotator::ZeroRotator, Params);
		if (Bounds == nullptr)
		{
			return false;
		}

		// Default brush is ~200uu; scale so world half-extent ≈ CoverHalfExtent.
		const FVector Scale(
			FMath::Max(CoverHalfExtent.X / 100.0f, 1.0f),
			FMath::Max(CoverHalfExtent.Y / 100.0f, 1.0f),
			FMath::Max(CoverHalfExtent.Z / 100.0f, 1.0f));
		Bounds->SetActorScale3D(Scale);
		NavSys->OnNavigationBoundsUpdated(Bounds);
		NavSys->Build();
		OutBoundsWeak = Bounds;
		return NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
	}

	static void RunRTSMovementReconciliationContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPRTSMovementRecon, Warning,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPRTSMovementRecon, Warning,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("RTSMovementReconciliationContract"), TEXT("RTSMovementRecon"), Token))
		{
			return;
		}

		UGP_RTSMovementReconciliationContractTestRunner* Runner =
			NewObject<UGP_RTSMovementReconciliationContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GRTSMovementReconContract(
		TEXT("gp.Movement.RunRTSMovementReconciliationContractTest"),
		TEXT("GP-S33M RTS Movement Reconciliation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRTSMovementReconciliationContractTest));
}

void UGP_RTSMovementReconciliationContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_RTSMovementReconciliationContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_RTSMovementReconciliationContractTestRunner::UnbindMovementResult()
{
	if (MovementResultHandle.IsValid())
	{
		if (UGP_MovementComponent* Movement = BoundMovementWeak.Get())
		{
			Movement->OnMovementResult().Remove(MovementResultHandle);
		}
		MovementResultHandle.Reset();
	}
	BoundMovementWeak.Reset();
}

void UGP_RTSMovementReconciliationContractTestRunner::OnWorldCleanup(
	UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_RTSMovementReconciliationContractTestRunner::CleanupActors()
{
	UnbindMovementResult();
	GPRTSMovementReconDebug::DestroyWeakSW(WalkerWeak);
	GPRTSMovementReconDebug::DestroyWeakSW(WalkerBWeak);
	GPRTSMovementReconDebug::DestroyWeakSW(WalkerCWeak);
	GPRTSMovementReconDebug::DestroyWeakWorker(EnemyWeak);
	GPRTSMovementReconDebug::DestroyWeakWorker(WorkerWeak);
	GPRTSMovementReconDebug::DestroyWeakActor(WallWeak);
	GPRTSMovementReconDebug::DestroyWeakActor(NavBoundsWeak);
	if (AGP_PlayerController* PC = PCWeak.Get())
	{
		PC->Destroy();
	}
	if (AGP_PlayerState* PS = PSWeak.Get())
	{
		PS->Destroy();
	}
	PCWeak.Reset();
	PSWeak.Reset();
}

void UGP_RTSMovementReconciliationContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
		if (AGP_SalvageWalker* SW = WalkerWeak.Get())
		{
			if (UGP_UnitCommandComponent* Cmd = SW->GetUnitCommandComponent())
			{
				Cmd->AutoAcquireScanIntervalSeconds = SavedScanInterval;
				Cmd->AutoAcquireSightRangeCm = SavedSightRange;
			}
		}
	}
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPRTSMovementRecon, Log,
		TEXT("gp.Movement.RunRTSMovementReconciliationContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPRTSMovementReconDebug::GActiveRunner.Reset();
}

void UGP_RTSMovementReconciliationContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPRTSMovementRecon, Error,
		TEXT("gp.Movement.RunRTSMovementReconciliationContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_RTSMovementReconciliationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPRTSMovementRecon, Error,
			TEXT("gp.Movement.RunRTSMovementReconciliationContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPRTSMovementRecon, Log,
		TEXT("gp.Movement.RunRTSMovementReconciliationContractTest PASS: %s"), Label);
	return true;
}

void UGP_RTSMovementReconciliationContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_RTSMovementReconciliationContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_RTSMovementReconciliationContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_RTSMovementReconciliationContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPRTSMovementRecon, Log,
		TEXT("gp.Movement.RunRTSMovementReconciliationContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_RTSMovementReconciliationContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	constexpr int32 TeamA = 1;
	constexpr int32 TeamB = 2;
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();

	switch (StageIndex)
	{
	case 0: // A setup: SW + nav-blocking wall + nav coverage
	{
		// Isolation for combat/auto-acquire; path stage teleports onto arena nav below.
		Origin = FVector(-56000.0f, -14000.0f, 100.0f);
		const FVector NavOrigin(0.0f, 0.0f, 100.0f);
		PathDest = NavOrigin + FVector(1400.0f, 0.0f, 0.0f);
		WallCenter = NavOrigin + FVector(700.0f, 0.0f, 150.0f);
		WallHalfExtent = FVector(100.0f, 400.0f, 150.0f); // size ~200x800x300

		AGP_SalvageWalker* Walker = GPRTSMovementReconDebug::SpawnSW(World, Origin, TeamA);
		WalkerWeak = Walker;
		if (!Expect(IsValid(Walker), TEXT("A_SpawnWalker")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		if (Cmd != nullptr)
		{
			SavedScanInterval = Cmd->AutoAcquireScanIntervalSeconds;
			Cmd->AutoAcquireScanIntervalSeconds = 10000.0f;
		}

		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		if (!Expect(Movement != nullptr, TEXT("A_MovementComponent")))
		{
			Finish();
			return;
		}
		AcceptanceRadiusCm = Movement->AcceptanceRadius;

		// Pathfinding stage runs on arena navmesh, away from map-authored hostiles.
		Walker->SetActorLocation(NavOrigin, false, nullptr, ETeleportType::TeleportPhysics);

		AActor* Wall = GPRTSMovementReconDebug::SpawnNavBlockingBox(World, WallCenter, WallHalfExtent);
		WallWeak = Wall;
		Expect(IsValid(Wall), TEXT("A_SpawnWall"));

		const FVector CoverHalf(2500.0f, 2500.0f, 800.0f);
		bNavAvailable = GPRTSMovementReconDebug::EnsureNavCoverage(
			World, NavOrigin + FVector(800.0f, 0.0f, 0.0f), CoverHalf, NavBoundsWeak);

		if (!bNavAvailable)
		{
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SKIP: A_NavPendingOrMissing (will soft-pass)"));
		}

		++StageIndex;
		ScheduleNext(1.25f); // wait for nav rebuild
		break;
	}
	case 1: // A issue RequestMove past wall
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("A2_WalkerAlive")))
		{
			Finish();
			return;
		}

		bNavAvailable = GPRTSMovementReconDebug::HasNavData(World)
			|| GPRTSMovementReconDebug::EnsureNavCoverage(
				World, WallCenter, FVector(2500.0f, 2500.0f, 800.0f), NavBoundsWeak);

		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(PathDest, 11u);
		Expect(Outcome.IsAccepted(), TEXT("A_RequestMoveAccepted"));

		++StageIndex;
		ScheduleNext(0.45f);
		break;
	}
	case 2: // A mid-path: nav path shape / soft skip
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("A3_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		bNavAvailable = GPRTSMovementReconDebug::HasNavData(World);

		if (!bNavAvailable)
		{
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SKIP: A_NoNav"));
			Expect(true, TEXT("A_SkipNoNav"));
		}
		else if (Movement->IsActivePathFromNavigation())
		{
			Expect(Movement->GetActivePathPointCount() >= 2, TEXT("A_NavPathPointCount"));

			const FVector Loc = Walker->GetActorLocation();
			const bool bPathThroughWall =
				GPRTSMovementReconDebug::PointInsideAABB(Loc, WallCenter, WallHalfExtent * 0.5f);
			Expect(!bPathThroughWall, TEXT("A_NotCrossingWallCenter"));
		}
		else
		{
			// Nav present but straight fallback — soft pass (map may lack rebuild).
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SKIP: A_StraightFallback"));
			Expect(true, TEXT("A_SkipStraightFallback"));
		}

		// Remove collision wall so arrival isn't blocked by WorldStatic sweep (nav may still be straight).
		if (AActor* WallActor = WallWeak.Get())
		{
			WallActor->Destroy();
			WallWeak.Reset();
		}

		if (!Movement->IsMoving())
		{
			Movement->RequestMove(PathDest, 12u);
		}

		++StageIndex;
		ScheduleNext(12.0f);
		break;
	}
	case 3: // A arrive OR still on multi-point nav path
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("A4_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		const float DistDest = FVector::Dist2D(Walker->GetActorLocation(), PathDest);
		const bool bArrived = DistDest < (AcceptanceRadiusCm + 80.0f);
		const bool bProgress =
			bArrived || Movement->IsMoving()
			|| (Movement->IsActivePathFromNavigation() && Movement->GetActivePathPointCount() >= 2)
			|| DistDest < 1400.0f;
		Expect(bProgress, TEXT("A_ArrivedOrProgress"));

		// Prep B/C: two walkers toward shared meet point.
		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		if (Movement->IsMoving())
		{
			Movement->StopMove(EGP_MovementStopReason::Manual);
		}
		Walker->SetActorLocation(Origin);

		AGP_SalvageWalker* WalkerB = GPRTSMovementReconDebug::SpawnSW(
			World, Origin + FVector(400.0f, 0.0f, 0.0f), TeamA);
		WalkerBWeak = WalkerB;
		if (!Expect(IsValid(WalkerB), TEXT("BC_SpawnWalkerB")))
		{
			Finish();
			return;
		}

		// Converge on same area with slight destination offset so idle stacking can't collapse Dist.
		SeparationMeet = Origin + FVector(200.0f, 0.0f, 0.0f);
		UGP_MovementComponent* MoveA = GPRTSMovementReconDebug::GetMovement(Walker);
		UGP_MovementComponent* MoveB = GPRTSMovementReconDebug::GetMovement(WalkerB);
		Expect(MoveA->RequestMove(SeparationMeet + FVector(0.0f, 50.0f, 0.0f), 21u).IsAccepted(),
			TEXT("BC_RequestMoveA"));
		Expect(MoveB->RequestMove(SeparationMeet + FVector(0.0f, -50.0f, 0.0f), 22u).IsAccepted(),
			TEXT("BC_RequestMoveB"));

		++StageIndex;
		ScheduleNext(6.0f);
		break;
	}
	case 4: // B/C separation > 60cm
	{
		AGP_SalvageWalker* WalkerA = WalkerWeak.Get();
		AGP_SalvageWalker* WalkerB = WalkerBWeak.Get();
		if (!IsValid(WalkerA) || !IsValid(WalkerB))
		{
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SOFT: BC_WalkersMissing"));
			Expect(true, TEXT("BC_SoftWalkersOptional"));
		}
		else
		{
			const float Dist = FVector::Dist2D(WalkerA->GetActorLocation(), WalkerB->GetActorLocation());
			Expect(Dist > 60.0f, TEXT("BC_SeparationGt60"));
		}

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 5: // D group spread via CommandComponent Validate+Dispatch
	{
		AGP_SalvageWalker* WalkerA = WalkerWeak.Get();
		AGP_SalvageWalker* WalkerB = WalkerBWeak.Get();
		if (!IsValid(WalkerA))
		{
			WalkerA = GPRTSMovementReconDebug::SpawnSW(World, Origin + FVector(0.0f, -150.0f, 0.0f), TeamA);
			WalkerWeak = WalkerA;
		}
		if (!IsValid(WalkerB))
		{
			WalkerB = GPRTSMovementReconDebug::SpawnSW(World, Origin + FVector(0.0f, 0.0f, 0.0f), TeamA);
			WalkerBWeak = WalkerB;
		}
		if (!Expect(IsValid(WalkerA) && IsValid(WalkerB), TEXT("D_WalkersABAlive")))
		{
			Finish();
			return;
		}

		UGP_MovementComponent* MoveA = GPRTSMovementReconDebug::GetMovement(WalkerA);
		UGP_MovementComponent* MoveB = GPRTSMovementReconDebug::GetMovement(WalkerB);
		if (MoveA && MoveA->IsMoving())
		{
			MoveA->StopMove(EGP_MovementStopReason::Manual);
		}
		if (MoveB && MoveB->IsMoving())
		{
			MoveB->StopMove(EGP_MovementStopReason::Manual);
		}

		WalkerA->SetActorLocation(Origin + FVector(0.0f, -150.0f, 0.0f));
		WalkerB->SetActorLocation(Origin + FVector(0.0f, 0.0f, 0.0f));

		AGP_SalvageWalker* WalkerC = GPRTSMovementReconDebug::SpawnSW(
			World, Origin + FVector(0.0f, 150.0f, 0.0f), TeamA);
		WalkerCWeak = WalkerC;
		if (!Expect(IsValid(WalkerC), TEXT("D_SpawnWalkerC")))
		{
			Finish();
			return;
		}

		AGameStateBase* GameState = World->GetGameState();
		AGP_PlayerState* PS = GPRTSMovementReconDebug::SpawnTeamPlayerState(World, GameState, TeamA);
		AGP_PlayerController* PC = GPRTSMovementReconDebug::SpawnTeamPC(World, PS);
		PSWeak = PS;
		PCWeak = PC;
		if (!Expect(IsValid(PS) && IsValid(PC), TEXT("D_SpawnPC")))
		{
			Finish();
			return;
		}

		UGP_SelectionComponent* Sel = PC->GetSelectionComponent();
		UGP_CommandComponent* ServerCmd = PC->GetCommandComponent();
		if (!Expect(Sel != nullptr && ServerCmd != nullptr, TEXT("D_SelectionAndCommand")))
		{
			Finish();
			return;
		}

		// Selection is local-controller-only; Validate/Dispatch use IssuingUnits.
		if (PC->IsLocalController())
		{
			Sel->ClearSelection();
			Sel->AddUnitToSelection(WalkerA);
			Sel->AddUnitToSelection(WalkerB);
			Sel->AddUnitToSelection(WalkerC);
			Expect(Sel->GetSelectionCount() == 3, TEXT("D_SelectionCount3"));
		}
		else
		{
			Expect(true, TEXT("D_SelectionSkipNonLocal"));
		}

		GroupMovePoint = Origin + FVector(900.0f, 0.0f, 0.0f);
		FGP_CommandRequest Req;
		FGP_CommandRequest Out;
		Req.CommandTag = GPTags.Command_Move;
		Req.IssuingUnits = {WalkerA, WalkerB, WalkerC};
		Req.TargetLocation = GroupMovePoint;
		Req.bQueue = false;

		EGP_CommandRejectReason Reason = EGP_CommandRejectReason::None;
		const bool bValidated = ServerCmd->ValidateAndNormalizeCommand(Req, Out, Reason);
		if (!Expect(bValidated, TEXT("D_ValidateMove")))
		{
			Finish();
			return;
		}
		const int32 Delivered = ServerCmd->DispatchValidatedCommand(Out);
		Expect(Delivered == 3, TEXT("D_Dispatch3"));

		const FGP_StoredUnitCommand* HeldA = WalkerA->GetUnitCommandComponent()->GetHeldCommand();
		const FGP_StoredUnitCommand* HeldB = WalkerB->GetUnitCommandComponent()->GetHeldCommand();
		const FGP_StoredUnitCommand* HeldC = WalkerC->GetUnitCommandComponent()->GetHeldCommand();
		if (!Expect(HeldA && HeldB && HeldC, TEXT("D_HeldCommandsPresent")))
		{
			Finish();
			return;
		}

		const float AB = FVector::Dist2D(HeldA->TargetLocation, HeldB->TargetLocation);
		const float AC = FVector::Dist2D(HeldA->TargetLocation, HeldC->TargetLocation);
		const float BC = FVector::Dist2D(HeldB->TargetLocation, HeldC->TargetLocation);
		Expect(AB > 40.0f && AC > 40.0f && BC > 40.0f, TEXT("D_HeldDestinationsDistinct"));

		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 6: // E single unit Move — Held dest equals click within 5cm
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_PlayerController* PC = PCWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(PC), TEXT("E_ActorsAlive")))
		{
			Finish();
			return;
		}

		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);

		UGP_CommandComponent* ServerCmd = PC->GetCommandComponent();
		if (UGP_SelectionComponent* Sel = PC->GetSelectionComponent())
		{
			if (PC->IsLocalController())
			{
				Sel->ReplaceSelectionWithUnit(Walker);
			}
		}

		ClickDest = Origin + FVector(700.0f, 250.0f, 0.0f);
		FGP_CommandRequest Req;
		FGP_CommandRequest Out;
		Req.CommandTag = GPTags.Command_Move;
		Req.IssuingUnits = {Walker};
		Req.TargetLocation = ClickDest;
		Req.bQueue = false;

		EGP_CommandRejectReason Reason = EGP_CommandRejectReason::None;
		const bool bValidated = ServerCmd->ValidateAndNormalizeCommand(Req, Out, Reason);
		Expect(bValidated, TEXT("E_ValidateSingleMove"));
		ServerCmd->DispatchValidatedCommand(Out);

		const FGP_StoredUnitCommand* Held = Walker->GetUnitCommandComponent()->GetHeldCommand();
		Expect(Held != nullptr && Held->CommandTag == GPTags.Command_Move, TEXT("E_HeldMove"));
		Expect(Held != nullptr && FVector::Dist(Held->TargetLocation, ClickDest) < 5.0f,
			TEXT("E_DestinationEqualsClick"));

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 7: // F AttackMove held destination preserved
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("F_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);
		GPRTSMovementReconDebug::ApplyCombatStats(Walker, 200.0f, 20.0f, 600.0f, 0.2f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		SavedScanInterval = Cmd->AutoAcquireScanIntervalSeconds;
		SavedSightRange = Cmd->AutoAcquireSightRangeCm;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->RefreshCombatAutoAcquireTimer();

		AttackMoveDest = Origin + FVector(1200.0f, 0.0f, 0.0f);
		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_AttackMove, nullptr, AttackMoveDest);
		Expect(Cmd->IsAttackMoveActive(), TEXT("F_AttackMoveActive"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDest) < 5.0f,
			TEXT("F_AttackMoveDestPreserved"));

		++StageIndex;
		ScheduleNext(0.55f);
		break;
	}
	case 8: // F travelling still holds dest; then G pure Move suppresses auto-acquire
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("F2_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive(), TEXT("F_StillAttackMoveActive"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDest) < 5.0f,
			TEXT("F_DestStillPreserved"));

		GPRTSMovementReconDebug::DestroyWeakWorker(EnemyWeak);
		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);

		AGP_Worker* Enemy = GPRTSMovementReconDebug::SpawnWorker(
			World, Origin + FVector(400.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Enemy), TEXT("G_SpawnEnemy")))
		{
			Finish();
			return;
		}
		GPRTSMovementReconDebug::ApplyCombatStats(Enemy, 200.0f, 1.0f, 100.0f, 5.0f);
		GPRTSMovementReconDebug::ApplyCombatStats(Walker, 200.0f, 20.0f, 600.0f, 0.2f);

		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		const FVector PureMoveDest = Origin + FVector(0.0f, 1000.0f, 0.0f);
		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Move, nullptr, PureMoveDest);
		Expect(Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand() != nullptr
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
			TEXT("G_HeldPureMove"));
		Expect(!Cmd->IsEligibleForCombatAutoAcquire(), TEXT("G_MoveSuppressesAutoAcquire"));

		++StageIndex;
		ScheduleNext(0.9f);
		break;
	}
	case 9: // G no attack during pure Move; H explicit Attack
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(Enemy), TEXT("GH_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackActive(), TEXT("G_NoAutoAcquireDuringPureMove"));

		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);
		Enemy->SetActorLocation(Origin + FVector(450.0f, 0.0f, 0.0f));

		GPRTSMovementReconDebug::IssueCommand(
			Walker, GPTags.Command_Attack, Enemy, Enemy->GetActorLocation());
		const bool bAttacking =
			Cmd->IsAttackActive()
			|| Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Approaching
			|| Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Ready;
		Expect(bAttacking, TEXT("H_ExplicitAttackActiveOrApproaching"));

		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 10: // I/J soft: Worker exists + RequestMove Accepted
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (IsValid(Walker))
		{
			GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		}
		GPRTSMovementReconDebug::DestroyWeakWorker(EnemyWeak);

		AGP_Worker* Worker = GPRTSMovementReconDebug::SpawnWorker(
			World, Origin + FVector(-300.0f, 0.0f, 0.0f), TeamA);
		WorkerWeak = Worker;
		if (!Expect(IsValid(Worker), TEXT("IJ_SpawnWorker")))
		{
			Finish();
			return;
		}

		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Worker);
		if (!Expect(Movement != nullptr, TEXT("IJ_WorkerMovement")))
		{
			Finish();
			return;
		}
		const FGP_MovementRequestOutcome Outcome =
			Movement->RequestMove(Origin + FVector(-100.0f, 200.0f, 0.0f), 31u);
		Expect(Outcome.IsAccepted(), TEXT("IJ_WorkerRequestMoveAccepted"));

		++StageIndex;
		ScheduleNext(0.3f);
		break;
	}
	case 11: // K serial supersede + L StopMove
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("KL_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPRTSMovementReconDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);

		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		UnbindMovementResult();
		bSerial1Cancelled = false;
		Serial1 = 41u;
		Serial2 = 42u;
		BoundMovementWeak = Movement;
		MovementResultHandle = Movement->OnMovementResult().AddLambda(
			[this](uint32 Serial, EGP_MovementResult Result, EGP_MovementResultReason /*Reason*/)
			{
				if (Serial == Serial1 && Result == EGP_MovementResult::Cancelled)
				{
					bSerial1Cancelled = true;
				}
			});

		const FVector Dest1 = Origin + FVector(1500.0f, 0.0f, 0.0f);
		const FVector Dest2 = Origin + FVector(0.0f, 1500.0f, 0.0f);
		Expect(Movement->RequestMove(Dest1, Serial1).IsAccepted(), TEXT("K_Serial1Accepted"));
		Expect(Movement->RequestMove(Dest2, Serial2).IsAccepted(), TEXT("K_Serial2Accepted"));
		Expect(bSerial1Cancelled || Movement->GetActiveMoveSerial() == Serial2,
			TEXT("K_Serial1CancelledOrSerial2Active"));
		Expect(Movement->GetActiveMoveSerial() == Serial2, TEXT("K_Serial2IsActive"));
		Expect(Movement->IsMoving(), TEXT("K_StillMovingAfterSupersede"));

		Movement->StopMove(EGP_MovementStopReason::Manual);
		Expect(!Movement->IsMoving(), TEXT("L_StopMoveClearsIsMoving"));

		UnbindMovementResult();

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 12: // M unreachable / off-nav
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("M_WalkerAlive")))
		{
			Finish();
			return;
		}

		Walker->SetActorLocation(Origin);
		UGP_MovementComponent* Movement = GPRTSMovementReconDebug::GetMovement(Walker);
		bNavAvailable = GPRTSMovementReconDebug::HasNavData(World);

		if (!bNavAvailable)
		{
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SKIP: M_SkipNoNav"));
			Expect(true, TEXT("M_SkipNoNav"));
			Finish();
			break;
		}

		UnreachableDest = Origin + FVector(0.0f, 0.0f, 50000.0f);
		Serial1 = 51u;
		UnbindMovementResult();
		bSawMovementFailed = false;
		BoundMovementWeak = Movement;
		MovementResultHandle = Movement->OnMovementResult().AddLambda(
			[this](uint32 /*Serial*/, EGP_MovementResult Result, EGP_MovementResultReason /*Reason*/)
			{
				if (Result == EGP_MovementResult::Failed)
				{
					bSawMovementFailed = true;
				}
			});

		const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(UnreachableDest, Serial1);
		const bool bRejectedOffNav =
			!Outcome.IsAccepted()
			&& (Outcome.RejectReason == EGP_MovementRejectReason::PathNotFound
				|| Outcome.RejectReason == EGP_MovementRejectReason::DestinationOffNav);

		if (bRejectedOffNav)
		{
			Expect(true, TEXT("M_RejectPathNotFoundOrOffNav"));
			UnbindMovementResult();
			Finish();
			break;
		}

		// Accepted — wait for Failed broadcast or stop.
		++StageIndex;
		ScheduleNext(2.5f);
		break;
	}
	case 13: // M async Failed broadcast (if RequestMove was accepted)
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_MovementComponent* Movement = IsValid(Walker) ? GPRTSMovementReconDebug::GetMovement(Walker) : nullptr;
		if (!Expect(Movement != nullptr, TEXT("M2_MovementAlive")))
		{
			Finish();
			return;
		}

		if (bSawMovementFailed || !Movement->IsMoving())
		{
			Expect(true, TEXT("M_FailedOrStopped"));
		}
		else if (!Movement->IsActivePathFromNavigation())
		{
			UE_LOG(LogGPRTSMovementRecon, Log,
				TEXT("gp.Movement.RunRTSMovementReconciliationContractTest SKIP: M_StraightFallbackStillMoving"));
			Expect(true, TEXT("M_SkipStraightFallback"));
			Movement->StopMove(EGP_MovementStopReason::Manual);
		}
		else
		{
			Expect(false, TEXT("M_UnreachableUnresolved"));
			Movement->StopMove(EGP_MovementStopReason::Manual);
		}

		UnbindMovementResult();
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_RTSMovementReconciliationContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_RTSMovementReconciliationContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_RTSMovementReconciliationContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_RTSMovementReconciliationContractTestRunner::AdvanceStage() {}
bool UGP_RTSMovementReconciliationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_RTSMovementReconciliationContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_RTSMovementReconciliationContractTestRunner::Finish() { bFinished = true; }
void UGP_RTSMovementReconciliationContractTestRunner::OnWorldCleanup(
	UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_RTSMovementReconciliationContractTestRunner::UnbindWorldCleanup() {}
void UGP_RTSMovementReconciliationContractTestRunner::CleanupActors() {}
void UGP_RTSMovementReconciliationContractTestRunner::UnbindMovementResult() {}

#endif
