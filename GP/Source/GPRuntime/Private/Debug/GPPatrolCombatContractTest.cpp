// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPPatrolCombatContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPatrolCombat, Log, All);

namespace GPPatrolCombatDebug
{
	static TWeakObjectPtr<UGP_PatrolCombatContractTestRunner> GActiveRunner;

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

	static void IssueCommand(AGP_UnitBase* Unit, const FGameplayTag& Tag, const FVector& Loc)
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
		Command.TargetLocation = Loc;
		Command.bQueue = false;
		Cmd->HandleCommand(Command);
	}

	static void DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
	{
		if (AGP_Worker* W = Weak.Get())
		{
			W->Destroy();
		}
		Weak.Reset();
	}

	static void NeutralizeAuthoredCombat(UWorld* World)
	{
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (IsValid(Unit) && !Unit->IsA<AGP_Worker>() && !Unit->IsA<AGP_SalvageWalker>())
			{
				Unit->SetTeamId(-1);
			}
		}
	}

	static void RunPatrolCombatContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPPatrolCombat, Warning,
				TEXT("gp.Combat.RunPatrolCombatContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPPatrolCombat, Warning,
				TEXT("gp.Combat.RunPatrolCombatContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("PatrolCombatContract"), TEXT("PatrolCombat"), Token))
		{
			return;
		}

		UGP_PatrolCombatContractTestRunner* Runner =
			NewObject<UGP_PatrolCombatContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GPatrolCombatContract(
		TEXT("gp.Combat.RunPatrolCombatContractTest"),
		TEXT("Patrol A↔B temporary combat engagement contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunPatrolCombatContractTest));
}

void UGP_PatrolCombatContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_PatrolCombatContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_PatrolCombatContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
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

void UGP_PatrolCombatContractTestRunner::CleanupActors()
{
	if (AGP_SalvageWalker* Walker = WalkerWeak.Get())
	{
		Walker->Destroy();
	}
	GPPatrolCombatDebug::DestroyWeakWorker(EnemyWeak);
	GPPatrolCombatDebug::DestroyWeakWorker(WorkerWeak);
	WalkerWeak.Reset();
}

void UGP_PatrolCombatContractTestRunner::Finish()
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
	UE_LOG(LogGPPatrolCombat, Log,
		TEXT("gp.Combat.RunPatrolCombatContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPPatrolCombatDebug::GActiveRunner.Reset();
}

void UGP_PatrolCombatContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPPatrolCombat, Error, TEXT("gp.Combat.RunPatrolCombatContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_PatrolCombatContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPPatrolCombat, Error, TEXT("gp.Combat.RunPatrolCombatContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPPatrolCombat, Log, TEXT("gp.Combat.RunPatrolCombatContractTest PASS: %s"), Label);
	return true;
}

void UGP_PatrolCombatContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldOnSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_PatrolCombatContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_PatrolCombatContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_PatrolCombatContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPPatrolCombat, Log, TEXT("gp.Combat.RunPatrolCombatContractTest Start"));
	AdvanceStage();
}

void UGP_PatrolCombatContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	constexpr int32 TeamA = 81;
	constexpr int32 TeamB = 82;
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();

	switch (StageIndex)
	{
	case 0:
	{
		GPPatrolCombatDebug::NeutralizeAuthoredCombat(World);
		Origin = FVector(-48000.0f, -12000.0f, 100.0f);
		PatrolDest = Origin + FVector(1400.0f, 0.0f, 0.0f);

		AGP_SalvageWalker* Walker = GPPatrolCombatDebug::SpawnSW(World, Origin, TeamA);
		AGP_Worker* Enemy = GPPatrolCombatDebug::SpawnWorker(
			World, Origin + FVector(500.0f, 0.0f, 0.0f), TeamB);
		WalkerWeak = Walker;
		EnemyWeak = Enemy;
		if (!Expect(IsValid(Walker) && IsValid(Enemy), TEXT("A0_SpawnActors")))
		{
			Finish();
			return;
		}

		GPPatrolCombatDebug::ApplyCombatStats(Walker, 200.0f, 8.0f, 600.0f, 0.25f);
		GPPatrolCombatDebug::ApplyCombatStats(Enemy, 80.0f, 1.0f, 100.0f, 5.0f);
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		if (!Expect(Cmd != nullptr, TEXT("A0_CommandComponent")))
		{
			Finish();
			return;
		}

		SavedScanInterval = Cmd->AutoAcquireScanIntervalSeconds;
		SavedSightRange = Cmd->AutoAcquireSightRangeCm;
		Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
		Cmd->AutoAcquireSightRangeCm = 900.0f;
		Cmd->RefreshCombatAutoAcquireTimer();

		GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Patrol, PatrolDest);
		Expect(Cmd->IsPatrolActive() && Cmd->IsEligibleForPatrolAcquire()
			&& !Cmd->IsEligibleForCombatAutoAcquire(),
			TEXT("A_PatrolHeldNotIdleAcquire"));
		Expect(Cmd->IsPatrolHeadingToB(), TEXT("A_HeadingToB"));
		WaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 1:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && IsValid(Enemy) && Cmd != nullptr, TEXT("A_ActorsAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (Cmd->IsPatrolEngaging()
			&& Cmd->IsPatrolActive()
			&& Cmd->GetAttackTarget() == Enemy
			&& Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Patrol)
		{
			Expect(true, TEXT("A_TemporaryAttackKeepsPatrolParent"));
			bSavedHeadingToB = Cmd->IsPatrolHeadingToB();
			GPPatrolCombatDebug::ApplyCombatStats(Enemy, 12.0f, 1.0f, 100.0f, 5.0f);
			GPPatrolCombatDebug::ApplyCombatStats(Walker, 200.0f, 40.0f, 600.0f, 0.12f);
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.25f);
			break;
		}

		if (WaitTicks > 24)
		{
			Expect(false, TEXT("A_TemporaryAttackKeepsPatrolParent"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 2:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("B_WalkerAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		const bool bEnemyGone = !IsValid(Enemy) || Enemy->IsDead();
		if (bEnemyGone && Cmd->IsPatrolActive() && !Cmd->IsAttackActive())
		{
			Expect(Cmd->IsPatrolHeadingToB() == bSavedHeadingToB, TEXT("B_ResumeSamePatrolLeg"));
			GPPatrolCombatDebug::DestroyWeakWorker(EnemyWeak);
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.25f);
			break;
		}

		if (WaitTicks > 28)
		{
			Expect(false, TEXT("B_ResumeSamePatrolLeg"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 3:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr && Cmd->IsPatrolActive(), TEXT("D_PatrolStillActive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (!Cmd->IsPatrolHeadingToB())
		{
			Expect(true, TEXT("D_ReachedBThenFlipTowardA"));
			AGP_Worker* Enemy2 = GPPatrolCombatDebug::SpawnWorker(
				World, Walker->GetActorLocation() + FVector(-400.0f, 0.0f, 0.0f), TeamB);
			EnemyWeak = Enemy2;
			if (!Expect(IsValid(Enemy2), TEXT("E_SpawnSecondEnemy")))
			{
				Finish();
				return;
			}
			GPPatrolCombatDebug::ApplyCombatStats(Walker, 200.0f, 8.0f, 600.0f, 0.25f);
			GPPatrolCombatDebug::ApplyCombatStats(Enemy2, 80.0f, 1.0f, 100.0f, 5.0f);
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.25f);
			break;
		}

		if (WaitTicks > 32)
		{
			Expect(false, TEXT("D_ReachedBThenFlipTowardA"));
			Finish();
			return;
		}
		ScheduleNext(0.35f);
		break;
	}
	case 4:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* Enemy = EnemyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("E_Actors")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (IsValid(Enemy) && Cmd->IsPatrolEngaging() && Cmd->GetAttackTarget() == Enemy)
		{
			Expect(true, TEXT("E_SecondEnemyEngagesAgain"));
			Enemy->Destroy();
			EnemyWeak.Reset();
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.25f);
			break;
		}

		if (WaitTicks > 24)
		{
			Expect(false, TEXT("E_SecondEnemyEngagesAgain"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 5:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("C_WalkerAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (Cmd->IsPatrolActive() && !Cmd->IsAttackActive())
		{
			Expect(true, TEXT("C_DestroyedEnemyResumesPatrol"));
			AGP_Worker* Enemy3 = GPPatrolCombatDebug::SpawnWorker(
				World, Walker->GetActorLocation() + FVector(350.0f, 0.0f, 0.0f), TeamB);
			EnemyWeak = Enemy3;
			GPPatrolCombatDebug::ApplyCombatStats(Enemy3, 200.0f, 1.0f, 100.0f, 5.0f);
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.25f);
			break;
		}

		if (WaitTicks > 20)
		{
			Expect(false, TEXT("C_DestroyedEnemyResumesPatrol"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 6:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("F_WalkerAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (Cmd->IsPatrolEngaging())
		{
			GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Stop, FVector::ZeroVector);
			Expect(!Cmd->IsPatrolActive() && !Cmd->IsAttackActive() && !Cmd->HasHeldCommand(),
				TEXT("F_StopClearsPatrolAndCombat"));
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.1f);
			break;
		}

		if (WaitTicks > 24)
		{
			Expect(false, TEXT("F_StopClearsPatrolAndCombat"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 7:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("G_WalkerAlive")))
		{
			Finish();
			return;
		}

		Walker->SetActorLocation(Origin);
		GPPatrolCombatDebug::DestroyWeakWorker(EnemyWeak);
		AGP_Worker* Enemy4 = GPPatrolCombatDebug::SpawnWorker(
			World, Origin + FVector(450.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy4;
		GPPatrolCombatDebug::ApplyCombatStats(Enemy4, 200.0f, 1.0f, 100.0f, 5.0f);
		GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Patrol, PatrolDest);
		WaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 8:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("G2_WalkerAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (Cmd->IsPatrolEngaging())
		{
			GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Move, Origin + FVector(0.0f, 800.0f, 0.0f));
			Expect(!Cmd->IsPatrolActive()
				&& Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
				TEXT("G_MoveReplacesPatrolCombat"));
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.1f);
			break;
		}

		if (WaitTicks > 24)
		{
			Expect(false, TEXT("G_MoveReplacesPatrolCombat"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 9:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("H_WalkerAlive")))
		{
			Finish();
			return;
		}

		Walker->SetActorLocation(Origin);
		GPPatrolCombatDebug::DestroyWeakWorker(EnemyWeak);
		AGP_Worker* Enemy5 = GPPatrolCombatDebug::SpawnWorker(
			World, Origin + FVector(450.0f, 0.0f, 0.0f), TeamB);
		EnemyWeak = Enemy5;
		GPPatrolCombatDebug::ApplyCombatStats(Enemy5, 200.0f, 1.0f, 100.0f, 5.0f);
		GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Patrol, PatrolDest);
		WaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 10:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Walker) && Cmd != nullptr, TEXT("H2_WalkerAlive")))
		{
			Finish();
			return;
		}

		++WaitTicks;
		if (Cmd->IsPatrolEngaging())
		{
			GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_AttackMove, PatrolDest);
			Expect(!Cmd->IsPatrolActive() && Cmd->IsAttackMoveActive(),
				TEXT("H_AttackMoveDoesNotResumePatrol"));
			WaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.1f);
			break;
		}

		if (WaitTicks > 24)
		{
			Expect(false, TEXT("H_AttackMoveDoesNotResumePatrol"));
			Finish();
			return;
		}
		ScheduleNext(0.25f);
		break;
	}
	case 11:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Stop, FVector::ZeroVector);
		GPPatrolCombatDebug::DestroyWeakWorker(EnemyWeak);

		AGP_Worker* Worker = GPPatrolCombatDebug::SpawnWorker(
			World, Origin + FVector(0.0f, 400.0f, 0.0f), TeamA);
		AGP_Worker* Enemy6 = GPPatrolCombatDebug::SpawnWorker(
			World, Origin + FVector(300.0f, 400.0f, 0.0f), TeamB);
		WorkerWeak = Worker;
		EnemyWeak = Enemy6;
		if (!Expect(IsValid(Worker) && IsValid(Enemy6), TEXT("I_SpawnWorkerAndEnemy")))
		{
			Finish();
			return;
		}

		GPPatrolCombatDebug::IssueCommand(Worker, GPTags.Command_Patrol, Origin + FVector(900.0f, 400.0f, 0.0f));
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd != nullptr && Cmd->IsPatrolActive() && !Cmd->IsEligibleForPatrolAcquire(),
			TEXT("I_WorkerPatrolNotCombatEligible"));
		WaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.8f);
		break;
	}
	case 12:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Worker != nullptr ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd != nullptr, TEXT("I2_WorkerAlive")))
		{
			Finish();
			return;
		}

		Expect(Cmd->IsPatrolActive() && !Cmd->IsAttackActive(),
			TEXT("I_WorkerKeepsPatrollingWithoutAcquire"));

		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (IsValid(Walker))
		{
			GPPatrolCombatDebug::IssueCommand(Walker, GPTags.Command_Patrol, PatrolDest);
			Expect(Walker->GetUnitCommandComponent() != nullptr
				&& Walker->GetUnitCommandComponent()->IsPatrolActive(),
				TEXT("J_PatrolBeforeDestroy"));
			Walker->Destroy();
			WalkerWeak.Reset();
			Expect(!IsValid(Walker), TEXT("J_DeathDestructionCleanup"));
		}

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#endif
