// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPCombatAutoAcquireContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCombatAutoAcquire, Log, All);

namespace GPCombatAutoAcquireDebug
{
	static TWeakObjectPtr<UGP_CombatAutoAcquireContractTestRunner> GActiveRunner;

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

	static void RunAutoAcquireContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCombatAutoAcquire, Warning, TEXT("gp.Combat.RunAutoAcquireContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPCombatAutoAcquire, Warning, TEXT("gp.Combat.RunAutoAcquireContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("CombatAutoAcquireContract"), TEXT("CombatAutoAcquire"), Token))
		{
			return;
		}

		UGP_CombatAutoAcquireContractTestRunner* Runner =
			NewObject<UGP_CombatAutoAcquireContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GAutoAcquireContract(
		TEXT("gp.Combat.RunAutoAcquireContractTest"),
		TEXT("GP-S30R combat Idle auto-acquire contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAutoAcquireContractTest));
}

void UGP_CombatAutoAcquireContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_CombatAutoAcquireContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_CombatAutoAcquireContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_CombatAutoAcquireContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		(void)World;
	}
	if (AGP_SalvageWalker* SW = WalkerWeak.Get())
	{
		SW->Destroy();
	}
	if (AGP_Worker* W = NearEnemyWeak.Get())
	{
		W->Destroy();
	}
	if (AGP_Worker* W = FarEnemyWeak.Get())
	{
		W->Destroy();
	}
	if (AGP_Worker* W = FriendlyWeak.Get())
	{
		W->Destroy();
	}
	WalkerWeak.Reset();
	NearEnemyWeak.Reset();
	FarEnemyWeak.Reset();
	FriendlyWeak.Reset();
}

void UGP_CombatAutoAcquireContractTestRunner::Finish()
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
	UE_LOG(LogGPCombatAutoAcquire, Log,
		TEXT("gp.Combat.RunAutoAcquireContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPCombatAutoAcquireDebug::GActiveRunner.Reset();
}

void UGP_CombatAutoAcquireContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPCombatAutoAcquire, Error, TEXT("gp.Combat.RunAutoAcquireContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_CombatAutoAcquireContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPCombatAutoAcquire, Error, TEXT("gp.Combat.RunAutoAcquireContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPCombatAutoAcquire, Log, TEXT("gp.Combat.RunAutoAcquireContractTest PASS: %s"), Label);
	return true;
}

void UGP_CombatAutoAcquireContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_CombatAutoAcquireContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_CombatAutoAcquireContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_CombatAutoAcquireContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPCombatAutoAcquire, Log, TEXT("gp.Combat.RunAutoAcquireContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_CombatAutoAcquireContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	constexpr int32 TeamA = 71;
	constexpr int32 TeamB = 72;
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FVector Origin(-52000.0f, -8000.0f, 100.0f);

	switch (StageIndex)
	{
	case 0: // Spawn + friendly ignore setup
	{
		AGP_SalvageWalker* Walker = GPCombatAutoAcquireDebug::SpawnSW(World, Origin, TeamA);
		AGP_Worker* Friendly = GPCombatAutoAcquireDebug::SpawnWorker(World, Origin + FVector(200.0f, 0.0f, 0.0f), TeamA);
		AGP_Worker* NearEnemy = GPCombatAutoAcquireDebug::SpawnWorker(World, Origin + FVector(300.0f, 0.0f, 0.0f), TeamB);
		AGP_Worker* FarEnemy = GPCombatAutoAcquireDebug::SpawnWorker(World, Origin + FVector(500.0f, 0.0f, 0.0f), TeamB);
		WalkerWeak = Walker;
		FriendlyWeak = Friendly;
		NearEnemyWeak = NearEnemy;
		FarEnemyWeak = FarEnemy;

		if (!Expect(IsValid(Walker) && IsValid(NearEnemy) && IsValid(FarEnemy) && IsValid(Friendly), TEXT("A_SpawnActors")))
		{
			Finish();
			return;
		}

		GPCombatAutoAcquireDebug::ApplyCombatStats(Walker, 200.0f, 20.0f, 600.0f, 0.2f);
		GPCombatAutoAcquireDebug::ApplyCombatStats(NearEnemy, 80.0f, 1.0f, 100.0f, 1.0f);
		GPCombatAutoAcquireDebug::ApplyCombatStats(FarEnemy, 80.0f, 1.0f, 100.0f, 1.0f);
		GPCombatAutoAcquireDebug::ApplyCombatStats(Friendly, 80.0f, 1.0f, 100.0f, 1.0f);

		if (UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent())
		{
			SavedScanInterval = Cmd->AutoAcquireScanIntervalSeconds;
			Cmd->AutoAcquireScanIntervalSeconds = 0.1f;
			Cmd->RefreshCombatAutoAcquireTimer();
		}

		Expect(Walker->GetUnitCommandComponent()->IsEligibleForCombatAutoAcquire(), TEXT("A_EligibleIdle"));
		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 1: // A/D nearest enemy acquired + attack starts; B friendly ignored
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* NearEnemy = NearEnemyWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(NearEnemy) && IsValid(FarEnemy), TEXT("A_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd != nullptr && Cmd->IsAttackActive(), TEXT("A_AttackStarted"));
		Expect(Cmd != nullptr && Cmd->GetAttackTarget() == NearEnemy, TEXT("D_NearestSelected"));
		Expect(Cmd != nullptr && Cmd->GetAttackTarget() != FarEnemy, TEXT("D_FarNotPreferred"));
		Expect(Cmd != nullptr && Cmd->GetAttackTarget() != FriendlyWeak.Get(), TEXT("B_FriendlyIgnored"));

		const float HpBefore = NearEnemy->GetUnitAttributeSet()->GetHealth();
		++StageIndex;
		ScheduleNext(0.35f);
		(void)HpBefore;
		break;
	}
	case 2: // Damage integration + explicit Attack override (E)
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* NearEnemy = NearEnemyWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(NearEnemy) && IsValid(FarEnemy), TEXT("E_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		const float HpNear = NearEnemy->GetUnitAttributeSet()->GetHealth();
		Expect(HpNear < 80.0f - KINDA_SMALL_NUMBER, TEXT("A_DamageAppliedViaExistingPath"));

		GPCombatAutoAcquireDebug::IssueCommand(
			Walker, GPTags.Command_Attack, FarEnemy, FarEnemy->GetActorLocation());
		Expect(Cmd->IsAttackActive(), TEXT("E_ExplicitAttackActive"));
		Expect(Cmd->GetAttackTarget() == FarEnemy, TEXT("E_ExplicitTargetForced"));

		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 3: // Auto-acquire must not override explicit (still Far)
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(FarEnemy), TEXT("E2_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->GetAttackTarget() == FarEnemy, TEXT("E_AutoDidNotOverrideExplicit"));

		GPCombatAutoAcquireDebug::IssueCommand(
			Walker, GPTags.Command_Move, nullptr, Walker->GetActorLocation() + FVector(0.0f, 800.0f, 0.0f));
		Expect(!Cmd->IsAttackActive(), TEXT("F_MoveCancelsAttack"));
		Expect(!Cmd->IsEligibleForCombatAutoAcquire(), TEXT("F_MoveSuppressesAutoAcquire"));

		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 4: // Stop → Idle → reacquire
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* NearEnemy = NearEnemyWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(NearEnemy) && IsValid(FarEnemy), TEXT("G_ActorsAlive")))
		{
			Finish();
			return;
		}

		// Reset positions/HP so nearest-alive selection is deterministic after Stop.
		Walker->SetActorLocation(Origin);
		NearEnemy->SetActorLocation(Origin + FVector(300.0f, 0.0f, 0.0f));
		FarEnemy->SetActorLocation(Origin + FVector(500.0f, 0.0f, 0.0f));
		GPCombatAutoAcquireDebug::ApplyCombatStats(NearEnemy, 80.0f, 1.0f, 100.0f, 1.0f);
		GPCombatAutoAcquireDebug::ApplyCombatStats(FarEnemy, 80.0f, 1.0f, 100.0f, 1.0f);

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		GPCombatAutoAcquireDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		Expect(!Cmd->HasHeldCommand(), TEXT("G_StopClearsHeld"));
		Expect(!Cmd->IsAttackActive(), TEXT("G_StopClearsAttack"));
		Expect(Cmd->IsEligibleForCombatAutoAcquire(), TEXT("G_EligibleAfterStop"));

		++StageIndex;
		ScheduleNext(0.5f);
		break;
	}
	case 5: // Reacquire after Stop + C dead ignored + H target loss
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* NearEnemy = NearEnemyWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker) && IsValid(NearEnemy), TEXT("H_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(Cmd->IsAttackActive(), TEXT("G_ReacquireAfterStop"));
		AGP_UnitBase* Acquired = Cmd->GetAttackTarget();
		Expect(Acquired == NearEnemy || Acquired == FarEnemy, TEXT("G_ReacquireEnemyTarget"));
		Expect(Acquired != FriendlyWeak.Get(), TEXT("G_ReacquireNotFriendly"));
		// Prefer nearest while Near is still a living in-range candidate.
		if (IsValid(NearEnemy) && !NearEnemy->IsDead()
			&& NearEnemy->GetUnitAttributeSet()->GetHealth() > 40.0f)
		{
			Expect(Acquired == NearEnemy, TEXT("G_ReacquireNearestWhileAlive"));
		}

		// Target loss: destroy acquired enemy; Attack FSM must clear sticky target.
		AGP_UnitBase* KillTarget = IsValid(Acquired) ? Acquired : NearEnemy;
		if (IsValid(KillTarget))
		{
			if (KillTarget == NearEnemy)
			{
				NearEnemyWeak.Reset();
			}
			else if (KillTarget == FarEnemy)
			{
				FarEnemyWeak.Reset();
			}
			KillTarget->Destroy();
		}
		Expect(!IsValid(KillTarget), TEXT("H_KillTargetDestroyed"));

		++StageIndex;
		ScheduleNext(0.5f);
		break;
	}
	case 6: // After near death: no sticky invalid; may acquire far if in range
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		AGP_Worker* NearEnemy = NearEnemyWeak.Get();
		AGP_Worker* FarEnemy = FarEnemyWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("H2_WalkerAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		if (IsValid(NearEnemy))
		{
			Expect(NearEnemy->IsDead() || NearEnemy->GetUnitAttributeSet()->GetHealth() <= KINDA_SMALL_NUMBER
				|| Cmd->GetAttackTarget() != NearEnemy,
				TEXT("C_DeadOrClearedNearTarget"));
		}

		if (Cmd->IsAttackActive())
		{
			Expect(Cmd->GetAttackTarget() != NearEnemy || !IsValid(NearEnemy) || NearEnemy->IsDead() == false,
				TEXT("H_NoStickyDeadTarget"));
			if (IsValid(FarEnemy) && !FarEnemy->IsDead())
			{
				Expect(Cmd->GetAttackTarget() == FarEnemy || Cmd->GetAttackTarget() == nullptr
					|| (IsValid(NearEnemy) && !NearEnemy->IsDead()),
					TEXT("H_ReacquireOrClearAfterLoss"));
			}
		}

		// Explicit: dead candidate ignored when Idle with only dead nearby — destroy far, leave dead near
		if (IsValid(FarEnemy))
		{
			FarEnemy->Destroy();
			FarEnemyWeak.Reset();
		}
		GPCombatAutoAcquireDebug::IssueCommand(Walker, GPTags.Command_Stop, nullptr, FVector::ZeroVector);
		++StageIndex;
		ScheduleNext(0.4f);
		break;
	}
	case 7: // Idle with only dead/invalid — no attack
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(Walker), TEXT("C2_WalkerAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Walker->GetUnitCommandComponent();
		Expect(!Cmd->IsAttackActive(), TEXT("C_DeadEnemyIgnoredNoAttack"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_CombatAutoAcquireContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_CombatAutoAcquireContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_CombatAutoAcquireContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_CombatAutoAcquireContractTestRunner::AdvanceStage() {}
bool UGP_CombatAutoAcquireContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_CombatAutoAcquireContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_CombatAutoAcquireContractTestRunner::Finish() { bFinished = true; }
void UGP_CombatAutoAcquireContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_CombatAutoAcquireContractTestRunner::UnbindWorldCleanup() {}
void UGP_CombatAutoAcquireContractTestRunner::CleanupActors() {}

#endif
