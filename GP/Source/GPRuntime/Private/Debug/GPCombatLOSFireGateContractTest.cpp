// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatLOS.h"
#include "Command/GPUnitCommand.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPUnitCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPLOSFireGate, Log, All);

namespace GPLOSFireGateDebug
{
	static TWeakObjectPtr<UGP_LOSFireGateContractTestRunner> GActiveRunner;

	static void ApplyCombatStats(
		AGP_UnitBase* Unit,
		float Health,
		float MaxHealth,
		float Damage,
		float Armor,
		float Resistance,
		float Cooldown,
		float Range)
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
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetMaxHealthAttribute(), MaxHealth);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), Damage);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetArmorAttribute(), Armor);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageResistanceAttribute(), Resistance);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackCooldownAttribute(), Cooldown);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackRangeAttribute(), Range);
	}

	static AGP_Worker* SpawnWorker(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(AGP_Worker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Worker != nullptr)
		{
			Worker->SetTeamId(TeamId);
		}
		return Worker;
	}

	static AActor* SpawnVisibilityBlocker(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Blocker == nullptr)
		{
			return nullptr;
		}

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Blocker, TEXT("BlockerMesh"));
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->RegisterComponent();
		Blocker->SetRootComponent(Mesh);
		Mesh->SetWorldLocation(Loc);
		Mesh->SetWorldScale3D(FVector(4.0f, 4.0f, 8.0f));

		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(CubeMesh);
		}

		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		return Blocker;
	}

	static void IssueAttack(AGP_Worker* Attacker, AGP_UnitBase* Target)
	{
		if (Attacker == nullptr || Target == nullptr)
		{
			return;
		}
		UGP_UnitCommandComponent* Cmd = Attacker->GetUnitCommandComponent();
		if (Cmd == nullptr)
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Attack;
		Command.TargetActor = Target;
		Command.TargetLocation = Target->GetActorLocation();
		Command.bQueue = false;
		Cmd->HandleCommand(Command);
	}

	static float ReadHealth(const AGP_UnitBase* Unit)
	{
		const UGP_UnitAttributeSet* Attrs = Unit != nullptr ? Unit->GetUnitAttributeSet() : nullptr;
		return Attrs != nullptr ? Attrs->GetHealth() : -1.0f;
	}

	static void RunLOSFireGateContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPLOSFireGate, Warning, TEXT("GP Combat.RunLOSFireGateContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("LOSFireGateContract"), TEXT("LOSFireGate"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_LOSFireGateContractTestRunner* Runner =
			NewObject<UGP_LOSFireGateContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GLOSFireGateContract(
		TEXT("gp.Combat.RunLOSFireGateContractTest"),
		TEXT("Authority: GP-S29R LOS fire-gate Attack contract (clear/block/resume/cooldown/FF/death)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunLOSFireGateContractTest));
}

void UGP_LOSFireGateContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_LOSFireGateContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_LOSFireGateContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		bCancelled = true;
		CancelReason = TEXT("WorldCleanup");
		Finish();
	}
}

void UGP_LOSFireGateContractTestRunner::CleanupActors()
{
	auto DestroyWeak = [](auto& Weak)
	{
		if (Weak.IsValid())
		{
			Weak->Destroy();
			Weak.Reset();
		}
	};
	DestroyWeak(BlockerWeak);
	DestroyWeak(AttackerWeak);
	DestroyWeak(TargetWeak);
	DestroyWeak(FriendlyWeak);
}

void UGP_LOSFireGateContractTestRunner::Finish()
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
	UnbindWorldCleanup();
	CleanupActors();

	UE_LOG(LogGPLOSFireGate, Log,
		TEXT("GP Combat.RunLOSFireGateContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	if (GPLOSFireGateDebug::GActiveRunner.Get() == this)
	{
		GPLOSFireGateDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_LOSFireGateContractTestRunner::Abort(const TCHAR* Reason)
{
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPLOSFireGate, Error, TEXT("GP Combat.RunLOSFireGateContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_LOSFireGateContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPLOSFireGate, Error, TEXT("GP Combat.RunLOSFireGateContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPLOSFireGate, Log, TEXT("GP Combat.RunLOSFireGateContractTest PASS: %s"), Label);
	return true;
}

void UGP_LOSFireGateContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_LOSFireGateContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_LOSFireGateContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_LOSFireGateContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPLOSFireGate, Log, TEXT("GP Combat.RunLOSFireGateContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_LOSFireGateContractTestRunner::AdvanceStage()
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
		// Spawn hostile pair in clear LOS / in range.
		const FVector AttackerLoc(52000.0f, 52000.0f, 200.0f);
		const FVector TargetLoc(52150.0f, 52000.0f, 200.0f);
		AGP_Worker* Attacker = GPLOSFireGateDebug::SpawnWorker(World, AttackerLoc, 1);
		AGP_Worker* Target = GPLOSFireGateDebug::SpawnWorker(World, TargetLoc, 2);
		AttackerWeak = Attacker;
		TargetWeak = Target;
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("SpawnHostilePair")))
		{
			Finish();
			return;
		}

		GPLOSFireGateDebug::ApplyCombatStats(Attacker, 100.0f, 100.0f, 10.0f, 0.0f, 0.0f, 0.35f, 400.0f);
		GPLOSFireGateDebug::ApplyCombatStats(Target, 100.0f, 100.0f, 1.0f, 0.0f, 0.0f, 5.0f, 100.0f);

		Expect(GPCombatLOS::HasLineOfSight(World, Attacker, Target), TEXT("A_ClearLOSHelper"));
		GPLOSFireGateDebug::IssueAttack(Attacker, Target);
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("A_AttackAccepted"));
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 1:
	{
		// A — clear LOS + hostile + in range → damage.
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("A_ActorsAlive")))
		{
			Finish();
			return;
		}

		const float Health = GPLOSFireGateDebug::ReadHealth(Target);
		if (Health < 99.5f)
		{
			HealthAfterClearHit = Health;
			Expect(Health < 100.0f, TEXT("A_ClearLOSDamage"));
			Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("A_AttackRemainsActive"));
			++StageIndex;
			ScheduleNext(0.05f);
			return;
		}

		++PollTicks;
		if (PollTicks > 40)
		{
			Expect(false, TEXT("A_ClearLOSDamageTimeout"));
			Finish();
			return;
		}
		ScheduleNext(0.1f);
		break;
	}
	case 2:
	{
		// B — Visibility blocker → no damage; D Attack retained; E cooldown not spent as success cadence advance.
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("B_ActorsAlive")))
		{
			Finish();
			return;
		}

		const FVector Mid =
			(Attacker->GetActorLocation() + Target->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 40.0f);
		AActor* Blocker = GPLOSFireGateDebug::SpawnVisibilityBlocker(World, Mid);
		BlockerWeak = Blocker;
		if (!Expect(IsValid(Blocker), TEXT("B_SpawnBlocker")))
		{
			Finish();
			return;
		}

		Expect(!GPCombatLOS::HasLineOfSight(World, Attacker, Target), TEXT("B_LOSBlockedHelper"));
		HealthAtBlock = GPLOSFireGateDebug::ReadHealth(Target);
		NextHitTimeAtBlock = Attacker->GetUnitCommandComponent()->GetNextAttackHitTime();
		AttackSerialAtBlock = Attacker->GetUnitCommandComponent()->IsAttackActive() ? 1u : 0u;
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("D_AttackActiveBeforeBlockWait"));
		Expect(!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("B_LOSStateClearBeforeBlockedAttempt"));
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.5f);
		break;
	}
	case 3:
	{
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("B_ActorsAliveWait")))
		{
			Finish();
			return;
		}

		const float HealthNow = GPLOSFireGateDebug::ReadHealth(Target);
		Expect(FMath::IsNearlyEqual(HealthNow, HealthAtBlock, 0.05f), TEXT("B_BlockedNoDamage"));
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("D_BlockedLOSKeepsAttack"));
		Expect(
			Attacker->GetUnitCommandComponent()->GetAttackExecutionState() == EGP_AttackExecutionState::Ready
			|| Attacker->GetUnitCommandComponent()->GetAttackExecutionState() == EGP_AttackExecutionState::Approaching,
			TEXT("D_AttackNotTerminalFail"));

		const double NextHitNow = Attacker->GetUnitCommandComponent()->GetNextAttackHitTime();
		// Successful-hit cooldown is not re-scheduled on blocked attempts: NextHitTime must not jump forward.
		Expect(
			(NextHitNow <= NextHitTimeAtBlock + 0.01) || (NextHitTimeAtBlock < 0.0),
			TEXT("E_BlockedNoSuccessfulCooldownSpend"));

		++PollTicks;
		if (PollTicks < 3)
		{
			// Repeated blocked retries must keep latched diagnostic state (no gameplay change).
			Expect(Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("B_LOSStateStaysBlockedAcrossRetries"));
			Expect(FMath::IsNearlyEqual(GPLOSFireGateDebug::ReadHealth(Target), HealthAtBlock, 0.05f), TEXT("B_BlockedHealthStableAcrossRetries"));
			ScheduleNext(0.4f);
			return;
		}

		Expect(Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("B_LOSStateBlockedAfterWait"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		// C — blocker removed → same Attack intent resumes and damages.
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("C_ActorsAlive")))
		{
			Finish();
			return;
		}

		if (BlockerWeak.IsValid())
		{
			BlockerWeak->Destroy();
			BlockerWeak.Reset();
		}
		Expect(GPCombatLOS::HasLineOfSight(World, Attacker, Target), TEXT("C_LOSRestoredHelper"));
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("C_AttackIntentPreserved"));
		HealthAtBlock = GPLOSFireGateDebug::ReadHealth(Target);
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 5:
	{
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("C_ActorsAliveWait")))
		{
			Finish();
			return;
		}

		const float HealthNow = GPLOSFireGateDebug::ReadHealth(Target);
		if (HealthNow < HealthAtBlock - 0.5f)
		{
			Expect(true, TEXT("C_ResumeDamageAfterLOSRestore"));
			Expect(!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("C_LOSStateClearAfterRestoreHit"));
			++StageIndex;
			ScheduleNext(0.05f);
			return;
		}

		++PollTicks;
		if (PollTicks > 40)
		{
			Expect(false, TEXT("C_ResumeDamageTimeout"));
			Finish();
			return;
		}
		ScheduleNext(0.1f);
		break;
	}
	case 6:
	{
		// I — blocked diagnostic state resets on Attack replacement (new target command).
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("I_ActorsAlive")))
		{
			Finish();
			return;
		}

		const FVector Mid =
			(Attacker->GetActorLocation() + Target->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 40.0f);
		AActor* Blocker = GPLOSFireGateDebug::SpawnVisibilityBlocker(World, Mid);
		BlockerWeak = Blocker;
		if (!Expect(IsValid(Blocker), TEXT("I_SpawnBlocker")))
		{
			Finish();
			return;
		}
		Expect(!GPCombatLOS::HasLineOfSight(World, Attacker, Target), TEXT("I_LOSBlockedHelper"));
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.5f);
		break;
	}
	case 7:
	{
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("I_ActorsAliveWait")))
		{
			Finish();
			return;
		}

		if (!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked())
		{
			++PollTicks;
			if (PollTicks > 20)
			{
				Expect(false, TEXT("I_LOSBlockedStateTimeout"));
				Finish();
				return;
			}
			ScheduleNext(0.2f);
			return;
		}

		Expect(Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("I_LOSStateBlockedBeforeReplace"));

		const uint32 SerialBeforeReplace = Attacker->GetUnitCommandComponent()->GetActiveAttackSerial();

		// Place replacement out of range / clear of the mid blocker so Ready cannot re-latch LOS in the same frame.
		AGP_Worker* Replacement = GPLOSFireGateDebug::SpawnWorker(
			World, Attacker->GetActorLocation() + FVector(2500.0f, 200.0f, 0.0f), 2);
		FriendlyWeak = Replacement;
		if (!Expect(IsValid(Replacement), TEXT("I_SpawnReplacementTarget")))
		{
			Finish();
			return;
		}
		GPLOSFireGateDebug::ApplyCombatStats(Replacement, 100.0f, 100.0f, 1.0f, 0.0f, 0.0f, 5.0f, 100.0f);
		GPLOSFireGateDebug::IssueAttack(Attacker, Replacement);
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("I_AttackReplacedAccepted"));
		Expect(
			Attacker->GetUnitCommandComponent()->GetActiveAttackSerial() != SerialBeforeReplace,
			TEXT("I_AttackSerialChangedOnReplace"));
		Expect(
			Attacker->GetUnitCommandComponent()->GetAttackTarget() == Replacement,
			TEXT("I_AttackTargetReplaced"));
		Expect(!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("I_LOSStateResetOnAttackReplace"));

		if (BlockerWeak.IsValid())
		{
			BlockerWeak->Destroy();
			BlockerWeak.Reset();
		}
		if (TargetWeak.IsValid())
		{
			TargetWeak->Destroy();
		}
		TargetWeak = Replacement;
		FriendlyWeak.Reset();

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		// F — out-of-range existing approach preserved.
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("F_ActorsAlive")))
		{
			Finish();
			return;
		}

		Target->SetActorLocation(Attacker->GetActorLocation() + FVector(2500.0f, 0.0f, 0.0f));
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.2f);
		break;
	}
	case 9:
	{
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Target = TargetWeak.Get();
		if (!Expect(IsValid(Attacker) && IsValid(Target), TEXT("F_ActorsAliveWait")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Attacker->GetUnitCommandComponent();
		const EGP_AttackExecutionState State = Cmd->GetAttackExecutionState();
		Expect(
			State == EGP_AttackExecutionState::Approaching || State == EGP_AttackExecutionState::Ready,
			TEXT("F_OutOfRangeApproachPreserved"));
		Expect(Cmd->IsAttackActive(), TEXT("F_AttackStillActiveOutOfRange"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10:
	{
		// G — same-team target rejected.
		AGP_Worker* Attacker = AttackerWeak.Get();
		if (!Expect(IsValid(Attacker), TEXT("G_AttackerAlive")))
		{
			Finish();
			return;
		}

		if (TargetWeak.IsValid())
		{
			TargetWeak->Destroy();
			TargetWeak.Reset();
		}
		if (BlockerWeak.IsValid())
		{
			BlockerWeak->Destroy();
			BlockerWeak.Reset();
		}

		AGP_Worker* Friendly = GPLOSFireGateDebug::SpawnWorker(
			World, Attacker->GetActorLocation() + FVector(120.0f, 0.0f, 0.0f), 1);
		FriendlyWeak = Friendly;
		if (!Expect(IsValid(Friendly), TEXT("G_SpawnFriendly")))
		{
			Finish();
			return;
		}

		GPLOSFireGateDebug::ApplyCombatStats(Friendly, 100.0f, 100.0f, 1.0f, 0.0f, 0.0f, 5.0f, 100.0f);
		GPLOSFireGateDebug::IssueAttack(Attacker, Friendly);
		Expect(!Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("G_SameTeamRejected"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11:
	{
		// H — death clears Attack.
		AGP_Worker* Attacker = AttackerWeak.Get();
		if (!Expect(IsValid(Attacker), TEXT("H_AttackerAlive")))
		{
			Finish();
			return;
		}

		AGP_Worker* Victim = GPLOSFireGateDebug::SpawnWorker(
			World, Attacker->GetActorLocation() + FVector(100.0f, 40.0f, 0.0f), 2);
		TargetWeak = Victim;
		if (!Expect(IsValid(Victim), TEXT("H_SpawnVictim")))
		{
			Finish();
			return;
		}

		GPLOSFireGateDebug::ApplyCombatStats(Attacker, 100.0f, 100.0f, 25.0f, 0.0f, 0.0f, 0.2f, 400.0f);
		GPLOSFireGateDebug::ApplyCombatStats(Victim, 100.0f, 100.0f, 1.0f, 0.0f, 0.0f, 5.0f, 100.0f);
		GPLOSFireGateDebug::IssueAttack(Attacker, Victim);
		Expect(Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("H_AttackStarted"));
		Expect(!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("H_LOSStateClearOnNewAttack"));
		PollTicks = 0;
		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 12:
	{
		AGP_Worker* Attacker = AttackerWeak.Get();
		AGP_Worker* Victim = TargetWeak.Get();
		if (!Expect(IsValid(Attacker), TEXT("H_AttackerAliveWait")))
		{
			Finish();
			return;
		}

		const bool bVictimDead = !IsValid(Victim) || Victim->IsDead();
		if (bVictimDead)
		{
			Expect(!Attacker->GetUnitCommandComponent()->IsAttackActive(), TEXT("H_DeathClearsAttack"));
			Expect(!Attacker->GetUnitCommandComponent()->IsAttackLOSBlocked(), TEXT("H_LOSStateClearedOnAttackEnd"));
			Expect(true, TEXT("SuiteComplete"));
			Finish();
			return;
		}

		++PollTicks;
		if (PollTicks > 50)
		{
			Expect(false, TEXT("H_DeathTimeout"));
			Finish();
			return;
		}
		ScheduleNext(0.1f);
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_LOSFireGateContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_LOSFireGateContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_LOSFireGateContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_LOSFireGateContractTestRunner::AdvanceStage() {}
bool UGP_LOSFireGateContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_LOSFireGateContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_LOSFireGateContractTestRunner::Finish() { bFinished = true; }
void UGP_LOSFireGateContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_LOSFireGateContractTestRunner::UnbindWorldCleanup() {}
void UGP_LOSFireGateContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
