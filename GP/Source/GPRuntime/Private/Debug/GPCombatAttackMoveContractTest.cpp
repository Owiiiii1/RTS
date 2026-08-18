// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPCombatAttackMoveContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPBuildingBase.h"
#include "Command/GPCommandComponent.h"
#include "Command/GPCommandRequest.h"
#include "Command/GPStoredUnitCommand.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildGridContractTest.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogGPCombatAttackMove, Log, All);

namespace GPCombatAttackMoveDebug
{
	static TWeakObjectPtr<UGP_CombatAttackMoveContractTestRunner> GActiveRunner;

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

	static bool IsUnitMoving(AGP_UnitBase* Unit)
	{
		AGP_MobileUnit* Mobile = Cast<AGP_MobileUnit>(Unit);
		if (Mobile == nullptr)
		{
			return false;
		}
		UGP_MovementComponent* Movement = Mobile->GetUnitMovementComponent();
		return Movement != nullptr && Movement->IsMoving();
	}

	static void DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
	{
		if (AGP_Worker* W = Weak.Get())
		{
			W->Destroy();
		}
		Weak.Reset();
	}

	static void RunAttackMoveContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCombatAttackMove, Warning, TEXT("gp.Combat.RunAttackMoveContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPCombatAttackMove, Warning, TEXT("gp.Combat.RunAttackMoveContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("CombatAttackMoveContract"), TEXT("CombatAttackMove"), Token))
		{
			return;
		}

		UGP_CombatAttackMoveContractTestRunner* Runner =
			NewObject<UGP_CombatAttackMoveContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GAttackMoveContract(
		TEXT("gp.Combat.RunAttackMoveContractTest"),
		TEXT("GP-S32A Attack-Move contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAttackMoveContractTest));
}

void UGP_CombatAttackMoveContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_CombatAttackMoveContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_CombatAttackMoveContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_CombatAttackMoveContractTestRunner::CleanupActors()
{
	if (AGP_SalvageWalker* SW = WalkerWeak.Get())
	{
		SW->Destroy();
	}
	GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
	GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyAltWeak);
	GPCombatAttackMoveDebug::DestroyWeakWorker(WorkerWeak);
	if (AGP_BuildingBase* Building = BuildingTargetWeak.Get())
	{
		Building->Destroy();
	}
	BuildingTargetWeak.Reset();
	if (AGP_PlayerController* PC = PCWeak.Get())
	{
		PC->Destroy();
	}
	if (AGP_PlayerState* PS = PSWeak.Get())
	{
		PS->Destroy();
	}
	WalkerWeak.Reset();
	PCWeak.Reset();
	PSWeak.Reset();
}

void UGP_CombatAttackMoveContractTestRunner::Finish()
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
	UE_LOG(LogGPCombatAttackMove, Log,
		TEXT("gp.Combat.RunAttackMoveContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPCombatAttackMoveDebug::GActiveRunner.Reset();
}

void UGP_CombatAttackMoveContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPCombatAttackMove, Error, TEXT("gp.Combat.RunAttackMoveContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_CombatAttackMoveContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPCombatAttackMove, Error, TEXT("gp.Combat.RunAttackMoveContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPCombatAttackMove, Log, TEXT("gp.Combat.RunAttackMoveContractTest PASS: %s"), Label);
	return true;
}

void UGP_CombatAttackMoveContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_CombatAttackMoveContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_CombatAttackMoveContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_CombatAttackMoveContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPCombatAttackMove, Log, TEXT("gp.Combat.RunAttackMoveContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_CombatAttackMoveContractTestRunner::AdvanceStage()
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
	case 0: // A setup: AttackMove to empty far destination
	{
		Origin = FVector(-48000.0f, -12000.0f, 100.0f);
		AttackMoveDestA = Origin + FVector(1500.0f, 0.0f, 0.0f);

		AGP_SalvageWalker* Walker = GPCombatAttackMoveDebug::SpawnSW(World, Origin, TeamA);
		WalkerWeak = Walker;
		if (!Expect(IsValid(Walker), TEXT("A_SpawnWalker")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 20.0f, 600.0f, 0.2f);
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		if (!Expect(Cmd != nullptr, TEXT("A_CommandComponent")))
		{
			Finish();
			return;
		}

		SavedScanInterval = Cmd->AutoAcquireScanIntervalSeconds;
		SavedSightRange = Cmd->AutoAcquireSightRangeCm;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AttackFacingRotationSpeedDegreesPerSecond = 360.0f;
		Cmd->RefreshCombatAutoAcquireTimer();

		Expect(Cmd->AttackFacingRotationSpeedDegreesPerSecond > KINDA_SMALL_NUMBER,
			TEXT("K_AttackFacingPropertyPresent"));

		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_AttackMove, nullptr, AttackMoveDestA);
		Expect(Cmd->IsAttackMoveActive(), TEXT("A_AttackMoveHeld"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestA) < 5.0f,
			TEXT("A_DestinationRecorded"));

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 1: // A travelling
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("A2_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive(), TEXT("A_StillAttackMoveActive"));
		Expect(GPCombatAttackMoveDebug::IsUnitMoving(Walker) || Cmd->IsAttackMoveActive(),
			TEXT("A_MovingOrHeldTravel"));
		Expect(!Cmd->IsAttackActive(), TEXT("A_NoEnemyNoEngage"));

		++StageIndex;
		// 1500cm @ 250cm/s ≈ 6s; generous buffer for arrival clear.
		ScheduleNext(8.0f);
		break;
	}
	case 2: // A arrived → Idle
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("A3_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackMoveActive(), TEXT("A_ArrivedClearsAttackMove"));
		Expect(!Cmd->HasHeldCommand(), TEXT("A_IdleNoHeld"));
		Expect(!Cmd->IsAttackActive(), TEXT("A_IdleNoAttack"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 3: // B/C setup: AttackMove past enemy in sight
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("B_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		Walker->SetActorLocation(Origin);
		Walker->SetActorRotation(FRotator::ZeroRotator);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 5.0f, 600.0f, 0.5f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		AttackMoveDestB = Origin + FVector(1600.0f, 0.0f, 0.0f);
		AGP_Worker* Enemy = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(700.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Enemy), TEXT("B_SpawnEnemy")))
		{
			Finish();
			return;
		}
		// High HP so C can observe OOR without immediate kill.
		GPCombatAttackMoveDebug::ApplyCombatStats(Enemy, 500.0f, 1.0f, 100.0f, 5.0f);
		EnemyHpAtAcquire = Enemy->GetUnitAttributeSet()->GetHealth();

		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_AttackMove, nullptr, AttackMoveDestB);
		Expect(Cmd->IsAttackMoveActive(), TEXT("B_AttackMoveIssued"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestB) < 5.0f,
			TEXT("B_OriginalDestinationStored"));

		++StageIndex;
		ScheduleNext(0.55f);
		break;
	}
	case 4: // B acquire while travelling
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(Enemy), TEXT("B2_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive(), TEXT("B_AttackMoveStillActive"));
		Expect(Cmd->IsAttackMoveEngaging() || Cmd->IsAttackActive(), TEXT("B_AcquiredWhileTravelling"));
		Expect(Cmd->GetAttackTarget() == Enemy, TEXT("B_EngageTargetIsEnemy"));

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 5: // C Approaching / no OOR damage
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(Enemy), TEXT("C_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackActive(), TEXT("C_AttackActive"));
		Expect(Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Approaching
			|| Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Ready,
			TEXT("C_ApproachOrReady"));

		const float Dist = FVector::Dist(Walker->GetActorLocation(), Enemy->GetActorLocation());
		const float HpNow = Enemy->GetUnitAttributeSet()->GetHealth();
		if (Dist > Cmd->GetAttackRange() + 5.0f)
		{
			Expect(Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Approaching
				|| Cmd->GetAttackExecutionState() == EGP_AttackExecutionState::Ready,
				TEXT("C_OutsideRangeApproaching"));
			Expect(FMath::IsNearlyEqual(HpNow, EnemyHpAtAcquire, 0.5f),
				TEXT("C_NoDamageOutsideAttackRange"));
		}

		// Prep D: low HP enemy + faster cadence so kill completes quickly.
		GPCombatAttackMoveDebug::ApplyCombatStats(Enemy, 15.0f, 1.0f, 100.0f, 5.0f);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 40.0f, 600.0f, 0.15f);
		EnemyHpAtAcquire = Enemy->GetUnitAttributeSet()->GetHealth();

		++StageIndex;
		// Approach from ~700cm @ 250 + kill + resume travel start.
		ScheduleNext(5.5f);
		break;
	}
	case 6: // D after kill → resume original destination
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("D_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();

		const bool bEnemyGone = !IsValid(Enemy) || Enemy->IsDead()
			|| Enemy->GetUnitAttributeSet()->GetHealth() <= KINDA_SMALL_NUMBER;
		Expect(bEnemyGone || !Cmd->IsAttackActive() || Cmd->GetAttackTarget() != Enemy,
			TEXT("D_EnemyKilledOrCleared"));

		Expect(Cmd->IsAttackMoveActive()
			|| FVector::Dist2D(Walker->GetActorLocation(), AttackMoveDestB) < 80.0f,
			TEXT("D_ResumeAttackMoveOrArrived"));
		if (Cmd->IsAttackMoveActive())
		{
			Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestB) < 5.0f,
				TEXT("D_OriginalDestinationPreserved"));
			Expect(GPCombatAttackMoveDebug::IsUnitMoving(Walker)
				|| FVector::Dist2D(Walker->GetActorLocation(), AttackMoveDestB) < 80.0f,
				TEXT("D_MovingTowardOriginalDest"));
		}

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 7: // E setup: AttackMove engage then explicit Move
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("E_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);
		Walker->SetActorRotation(FRotator::ZeroRotator);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 5.0f, 600.0f, 0.5f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		AGP_Worker* Enemy = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(500.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Enemy), TEXT("E_SpawnEnemy")))
		{
			Finish();
			return;
		}
		GPCombatAttackMoveDebug::ApplyCombatStats(Enemy, 400.0f, 1.0f, 100.0f, 5.0f);

		ExplicitMoveDest = Origin + FVector(0.0f, 900.0f, 0.0f);
		GPCombatAttackMoveDebug::IssueCommand(
			Walker, GPTags.Command_AttackMove, nullptr, Origin + FVector(1600.0f, 0.0f, 0.0f));

		++StageIndex;
		ScheduleNext(0.55f);
		break;
	}
	case 8: // E explicit Move cancels AttackMove
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(Enemy), TEXT("E2_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive() || Cmd->IsAttackActive(), TEXT("E_EngagedOrTravelling"));

		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Move, nullptr, ExplicitMoveDest);
		Expect(!Cmd->IsAttackMoveActive(), TEXT("E_MoveCancelsAttackMove"));
		Expect(!Cmd->IsAttackActive(), TEXT("E_MoveCancelsAttack"));
		Expect(Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand() != nullptr
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
			TEXT("E_HeldMoveAfterCancel"));

		++StageIndex;
		ScheduleNext(1.0f);
		break;
	}
	case 9: // E no later AttackMove resume
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("E3_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackMoveActive(), TEXT("E_NoAttackMoveResume"));
		Expect(
			!Cmd->HasHeldCommand()
			|| (Cmd->GetHeldCommand() != nullptr && Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move)
			|| !GPCombatAttackMoveDebug::IsUnitMoving(Walker),
			TEXT("E_HeldMoveOrIdleAfterMove"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 10: // F setup: AttackMove then explicit Attack
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("F_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyAltWeak);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);
		Walker->SetActorRotation(FRotator::ZeroRotator);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 5.0f, 600.0f, 0.5f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		AGP_Worker* EnemyA = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(450.0f, 0.0f, 0.0f), TeamB);
		AGP_Worker* EnemyB = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(0.0f, 450.0f, 0.0f), TeamB);
		EnemyWeak = EnemyA;
		EnemyAltWeak = EnemyB;
		if (!Expect(IsValid(EnemyA) && IsValid(EnemyB), TEXT("F_SpawnEnemies")))
		{
			Finish();
			return;
		}
		GPCombatAttackMoveDebug::ApplyCombatStats(EnemyA, 400.0f, 1.0f, 100.0f, 5.0f);
		GPCombatAttackMoveDebug::ApplyCombatStats(EnemyB, 20.0f, 1.0f, 100.0f, 5.0f);

		GPCombatAttackMoveDebug::IssueCommand(
			Walker, GPTags.Command_AttackMove, nullptr, Origin + FVector(1600.0f, 0.0f, 0.0f));

		++StageIndex;
		ScheduleNext(0.55f);
		break;
	}
	case 11: // F explicit Attack cancels AttackMove
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* EnemyA = EnemyWeak.Get();
		AGP_Worker* EnemyB = EnemyAltWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(EnemyA) && IsValid(EnemyB), TEXT("F2_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive() || Cmd->IsAttackActive(), TEXT("F_AttackMoveActiveBeforeExplicit"));

		GPCombatAttackMoveDebug::IssueCommand(
			Walker, GPTags.Command_Attack, EnemyB, EnemyB->GetActorLocation());
		Expect(!Cmd->IsAttackMoveActive(), TEXT("F_ExplicitAttackCancelsAttackMove"));
		Expect(Cmd->IsAttackActive(), TEXT("F_ExplicitAttackActive"));
		Expect(Cmd->GetAttackTarget() == EnemyB, TEXT("F_ExplicitTargetEngaged"));

		// Force quick death of explicit target.
		GPCombatAttackMoveDebug::ApplyCombatStats(EnemyB, 5.0f, 1.0f, 100.0f, 5.0f);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 50.0f, 600.0f, 0.1f);

		++StageIndex;
		ScheduleNext(2.5f);
		break;
	}
	case 12: // F after explicit target death — no AttackMove resume
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("F3_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackMoveActive(), TEXT("F_NoAttackMoveResumeAfterExplicitKill"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 13: // G Stop cancels AttackMove + attack → Idle
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("G_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyAltWeak);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);

		AGP_Worker* Enemy = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(400.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Enemy), TEXT("G_SpawnEnemy")))
		{
			Finish();
			return;
		}
		GPCombatAttackMoveDebug::ApplyCombatStats(Enemy, 300.0f, 1.0f, 100.0f, 5.0f);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 5.0f, 600.0f, 0.5f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		GPCombatAttackMoveDebug::IssueCommand(
			Walker, GPTags.Command_AttackMove, nullptr, Origin + FVector(1600.0f, 0.0f, 0.0f));

		++StageIndex;
		ScheduleNext(0.55f);
		break;
	}
	case 14: // G Stop while AttackMove active
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("G2_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackMoveActive() || Cmd->IsAttackActive(), TEXT("G_AttackMoveOrEngageBeforeStop"));

		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Expect(!Cmd->IsAttackMoveActive(), TEXT("G_StopCancelsAttackMove"));
		Expect(!Cmd->IsAttackActive(), TEXT("G_StopCancelsAttack"));
		Expect(!Cmd->HasHeldCommand(), TEXT("G_IdleAfterStop"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 15: // H new AttackMove replaces prior destination
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("H_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		Walker->SetActorLocation(Origin);
		AttackMoveDestH1 = Origin + FVector(1200.0f, 0.0f, 0.0f);
		AttackMoveDestH2 = Origin + FVector(0.0f, 1400.0f, 0.0f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_AttackMove, nullptr, AttackMoveDestH1);
		Expect(Cmd->IsAttackMoveActive(), TEXT("H_FirstAttackMoveActive"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestH1) < 5.0f,
			TEXT("H_FirstDestination"));

		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_AttackMove, nullptr, AttackMoveDestH2);
		Expect(Cmd->IsAttackMoveActive(), TEXT("H_SecondAttackMoveActive"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestH2) < 5.0f,
			TEXT("H_DestinationReplaced"));
		Expect(FVector::Dist(Cmd->GetAttackMoveDestination(), AttackMoveDestH1) > 50.0f,
			TEXT("H_OldDestinationGone"));

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 16: // I pure Move does not auto-acquire
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("I_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Walker->SetActorLocation(Origin);
		Walker->SetActorRotation(FRotator::ZeroRotator);
		GPCombatAttackMoveDebug::ApplyCombatStats(Walker, 200.0f, 20.0f, 600.0f, 0.2f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->RefreshCombatAutoAcquireTimer();

		AGP_Worker* Enemy = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(400.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Enemy), TEXT("I_SpawnEnemy")))
		{
			Finish();
			return;
		}
		GPCombatAttackMoveDebug::ApplyCombatStats(Enemy, 200.0f, 1.0f, 100.0f, 5.0f);

		const FVector MoveDest = Origin + FVector(0.0f, 1000.0f, 0.0f);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Move, nullptr, MoveDest);
		Expect(Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand() != nullptr
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
			TEXT("I_HeldPureMove"));
		Expect(!Cmd->IsEligibleForCombatAutoAcquire(), TEXT("I_MoveSuppressesAutoAcquire"));
		Expect(!Cmd->IsEligibleForAttackMoveAcquire(), TEXT("I_NotAttackMoveAcquireEligible"));

		++StageIndex;
		ScheduleNext(0.8f);
		break;
	}
	case 17: // I after wait — still no attack from pure Move
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("I2_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackActive(), TEXT("I_NoAutoAcquireDuringPureMove"));
		Expect(!Cmd->IsAttackMoveActive(), TEXT("I_NotAttackMove"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 18: // J Worker AttackMove rejected by ValidateAndNormalizeCommand
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("J_WalkerAlive")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);

		AGP_Worker* Worker = GPCombatAttackMoveDebug::SpawnWorker(
			World, Origin + FVector(-200.0f, 0.0f, 0.0f), TeamA);
		WorkerWeak = Worker;
		if (!Expect(IsValid(Worker), TEXT("J_SpawnWorker")))
		{
			Finish();
			return;
		}

		AGameStateBase* GameState = World->GetGameState();
		AGP_PlayerState* PS = GPCombatAttackMoveDebug::SpawnTeamPlayerState(World, GameState, TeamA);
		AGP_PlayerController* PC = GPCombatAttackMoveDebug::SpawnTeamPC(World, PS);
		PSWeak = PS;
		PCWeak = PC;
		if (!Expect(IsValid(PS) && IsValid(PC), TEXT("J_SpawnPC")))
		{
			Finish();
			return;
		}

		UGP_CommandComponent* ServerCmd = PC->GetCommandComponent();
		if (!Expect(ServerCmd != nullptr, TEXT("J_CommandComponent")))
		{
			Finish();
			return;
		}

		FGP_CommandRequest Req;
		FGP_CommandRequest Out;
		Req.CommandTag = GPTags.Command_AttackMove;
		Req.IssuingUnits = {Worker};
		Req.TargetLocation = Origin + FVector(800.0f, 0.0f, 0.0f);
		Req.bQueue = false;

		EGP_CommandRejectReason Reason = EGP_CommandRejectReason::None;
		const bool bAccepted = ServerCmd->ValidateAndNormalizeCommand(Req, Out, Reason);
		Expect(!bAccepted, TEXT("J_WorkerAttackMoveRejected"));
		Expect(Reason == EGP_CommandRejectReason::UnsupportedUnit, TEXT("J_UnsupportedUnitReason"));

		// Direct HandleCommand may still hold AttackMove on MobileUnit; combat acquire must stay ineligible.
		UGP_UnitCommandComponent* WorkerCmd = Worker->GetUnitCommandComponent();
		if (WorkerCmd != nullptr)
		{
			GPCombatAttackMoveDebug::IssueCommand(
				Worker, GPTags.Command_AttackMove, nullptr, Origin + FVector(800.0f, 0.0f, 0.0f));
			Expect(!WorkerCmd->IsEligibleForAttackMoveAcquire(),
				TEXT("J_WorkerNotEligibleAttackMoveAcquire"));
		}

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 19: // K soft facing + AttackMove input modal (local PC if available)
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("K_WalkerAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->AttackFacingRotationSpeedDegreesPerSecond > KINDA_SMALL_NUMBER,
			TEXT("K_FacingSpeedConfigured"));

		AGP_PlayerController* LocalPC = Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		if (IsValid(LocalPC) && LocalPC->IsLocalController())
		{
			if (UGP_SelectionComponent* Sel = LocalPC->GetSelectionComponent())
			{
				Sel->ReplaceSelectionWithUnit(Walker);
			}
			LocalPC->CancelAttackMoveMode();
			LocalPC->EnterAttackMoveMode();
			Expect(LocalPC->IsAttackMoveModeActive(), TEXT("K_EnterAttackMoveMode"));

			LocalPC->UpdateAttackMoveInputEdgesForContract(
				/*bLMBDown=*/false, /*bRMBDown=*/false, /*bADown=*/false, /*bEscDown=*/true);
			Expect(!LocalPC->IsAttackMoveModeActive(), TEXT("K_EscCancelsAttackMoveMode"));
		}
		else
		{
			UE_LOG(LogGPCombatAttackMove, Log,
				TEXT("gp.Combat.RunAttackMoveContractTest SKIP: K_InputMode (no local PC)"));
		}

		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyWeak);
		GPCombatAttackMoveDebug::DestroyWeakWorker(EnemyAltWeak);
		Walker->SetActorLocation(Origin);
		GPCombatAttackMoveDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_BuildingBase* Building = World->SpawnActor<AGP_BuildGridContractStub>(
			AGP_BuildGridContractStub::StaticClass(),
			Origin + FVector(400.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		if (IsValid(Building))
		{
			Building->SetActorLocation(Origin + FVector(400.0f, 0.0f, 0.0f));
			Building->SetTeamId(TeamB);
			GPCombatAttackMoveDebug::ApplyCombatStats(Building, 200.0f, 0.0f, 0.0f, 1.0f);
		}
		BuildingTargetWeak = Building;
		if (!Expect(IsValid(Building), TEXT("Legacy_SpawnAttackMoveBuilding")))
		{
			Finish();
			return;
		}

		GPCombatAttackMoveDebug::IssueCommand(
			Walker, GPTags.Command_AttackMove, nullptr, Origin + FVector(1600.0f, 0.0f, 0.0f));
		++StageIndex;
		ScheduleNext(0.8f);
		break;
	}
	case 20: // Legacy S32A: AttackMove acquire still excludes buildings
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_BuildingBase* Building = BuildingTargetWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(Building), TEXT("Legacy_AttackMoveBuildingAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		const UGP_UnitAttributeSet* Attrs = Building->GetUnitAttributeSet();
		const float BuildingHp = Attrs != nullptr ? Attrs->GetHealth() : -1.0f;
		Expect(Cmd != nullptr
			&& Cmd->GetAttackTarget() != Building
			&& BuildingHp >= 199.0f,
			TEXT("Legacy_AttackMoveAcquireExcludesBuildings"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_CombatAttackMoveContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_CombatAttackMoveContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_CombatAttackMoveContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_CombatAttackMoveContractTestRunner::AdvanceStage() {}
bool UGP_CombatAttackMoveContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_CombatAttackMoveContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_CombatAttackMoveContractTestRunner::Finish() { bFinished = true; }
void UGP_CombatAttackMoveContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_CombatAttackMoveContractTestRunner::UnbindWorldCleanup() {}
void UGP_CombatAttackMoveContractTestRunner::CleanupActors() {}

#endif
