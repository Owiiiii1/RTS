// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPCombatRetaliationPursuitContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Command/GPStoredUnitCommand.h"
#include "Command/GPUnitCommand.h"
#include "Combat/GPCombatLOS.h"
#include "Combat/GPDamageApplication.h"
#include "Components/StaticMeshComponent.h"
#include "Units/GPUnitBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCombatRetaliation, Log, All);

namespace GPCombatRetaliationDebug
{
	static TWeakObjectPtr<UGP_CombatRetaliationPursuitContractTestRunner> GActiveRunner;

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

	static void SilenceAutoAcquire(AGP_UnitBase* Unit)
	{
		if (UGP_UnitCommandComponent* Cmd = Unit != nullptr ? Unit->GetUnitCommandComponent() : nullptr)
		{
			Cmd->AutoAcquireScanIntervalSeconds = 10000.0f;
			Cmd->RefreshCombatAutoAcquireTimer();
		}
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
			SilenceAutoAcquire(SW);
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
			SilenceAutoAcquire(Worker);
		}
		return Worker;
	}

	static AActor* SpawnVisibilityBlocker(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
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

	static AGP_DefensiveTurret* SpawnTurret(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_DefensiveTurret* Turret = World->SpawnActor<AGP_DefensiveTurret>(
			AGP_DefensiveTurret::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Turret != nullptr)
		{
			Turret->SetTeamId(TeamId);
			SilenceAutoAcquire(Turret);
		}
		return Turret;
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

	static bool ApplyHit(AGP_UnitBase* Source, AGP_UnitBase* Target)
	{
		if (Source == nullptr || Target == nullptr)
		{
			return false;
		}
		FGP_DamageApplicationResult Result;
		return Target->ApplyDamageFromUnit(Source, Result);
	}

	static void RunRetaliationPursuitContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCombatRetaliation, Warning, TEXT("gp.Combat.RunRetaliationPursuitContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPCombatRetaliation, Warning, TEXT("gp.Combat.RunRetaliationPursuitContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("CombatRetaliationPursuitContract"), TEXT("CombatRetaliationPursuit"), Token))
		{
			return;
		}

		UGP_CombatRetaliationPursuitContractTestRunner* Runner =
			NewObject<UGP_CombatRetaliationPursuitContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GRetaliationPursuitContract(
		TEXT("gp.Combat.RunRetaliationPursuitContractTest"),
		TEXT("GP-S40R timed retaliation pursuit contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRetaliationPursuitContractTest));
}

void UGP_CombatRetaliationPursuitContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_CombatRetaliationPursuitContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_CombatRetaliationPursuitContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_CombatRetaliationPursuitContractTestRunner::CleanupActors()
{
	if (AGP_SalvageWalker* Actor = VictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = AttackerWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = AttackerBWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = EngageVictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = EngageAttackerWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = ManualVictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_Worker* Actor = WorkerVictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_DefensiveTurret* Actor = TurretWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = LOSVictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = LOSAttackerWeak.Get()) { Actor->Destroy(); }
	if (AActor* Actor = LOSBlockerWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = TimeoutLOSVictimWeak.Get()) { Actor->Destroy(); }
	if (AGP_SalvageWalker* Actor = TimeoutLOSAttackerWeak.Get()) { Actor->Destroy(); }
	if (AActor* Actor = TimeoutLOSBlockerWeak.Get()) { Actor->Destroy(); }
	VictimWeak.Reset();
	AttackerWeak.Reset();
	AttackerBWeak.Reset();
	EngageVictimWeak.Reset();
	EngageAttackerWeak.Reset();
	ManualVictimWeak.Reset();
	WorkerVictimWeak.Reset();
	TurretWeak.Reset();
	LOSVictimWeak.Reset();
	LOSAttackerWeak.Reset();
	LOSBlockerWeak.Reset();
	TimeoutLOSVictimWeak.Reset();
	TimeoutLOSAttackerWeak.Reset();
	TimeoutLOSBlockerWeak.Reset();
	ShortRetaliationDefWeak.Reset();
}

void UGP_CombatRetaliationPursuitContractTestRunner::Finish()
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
	UE_LOG(LogGPCombatRetaliation, Log,
		TEXT("gp.Combat.RunRetaliationPursuitContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPCombatRetaliationDebug::GActiveRunner.Reset();
}

void UGP_CombatRetaliationPursuitContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPCombatRetaliation, Error, TEXT("gp.Combat.RunRetaliationPursuitContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_CombatRetaliationPursuitContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPCombatRetaliation, Error, TEXT("gp.Combat.RunRetaliationPursuitContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPCombatRetaliation, Log, TEXT("gp.Combat.RunRetaliationPursuitContractTest PASS: %s"), Label);
	return true;
}

void UGP_CombatRetaliationPursuitContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_CombatRetaliationPursuitContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_CombatRetaliationPursuitContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_CombatRetaliationPursuitContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPCombatRetaliation, Log, TEXT("gp.Combat.RunRetaliationPursuitContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_CombatRetaliationPursuitContractTestRunner::AdvanceStage()
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
	Origin = FVector(-54000.0f, -12000.0f, 100.0f);

	switch (StageIndex)
	{
	case 0:
	{
		AGP_SalvageWalker* Victim = GPCombatRetaliationDebug::SpawnSW(World, Origin, TeamA);
		AGP_SalvageWalker* Attacker = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(2500.0f, 0.0f, 0.0f), TeamB);
		AGP_SalvageWalker* AttackerB = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(2600.0f, 400.0f, 0.0f), TeamB);
		VictimWeak = Victim;
		AttackerWeak = Attacker;
		AttackerBWeak = AttackerB;
		if (!Expect(IsValid(Victim) && IsValid(Attacker) && IsValid(AttackerB), TEXT("SpawnFarPair")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(Victim, 400.0f, 25.0f, 250.0f, 1.0f);
		GPCombatRetaliationDebug::ApplyCombatStats(Attacker, 400.0f, 15.0f, 250.0f, 1.0f);
		GPCombatRetaliationDebug::ApplyCombatStats(AttackerB, 400.0f, 15.0f, 250.0f, 1.0f);
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireSightRangeCm = 900.0f;
		}
		Expect(FMath::IsNearlyEqual(Victim->GetRetaliationPursuitSeconds(), 5.0f, 0.01f), TEXT("B_DefaultSeconds5"));
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, Victim), TEXT("A_DamageApplied"));
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsRetaliationActive(), TEXT("A_RetaliationStarted"));
			Expect(Cmd->GetRetaliationTarget() == Attacker, TEXT("A_SameAttacker"));
			Expect(!Cmd->IsAttackActive(), TEXT("A_NotYetAttackFSM"));
			Expect(!Cmd->HasHeldCommand(), TEXT("A_NoHeldCommand"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		if (!Expect(IsValid(Victim) && IsValid(Attacker), TEXT("B_ActorsAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitDefinition* ShortDef = NewObject<UGP_UnitDefinition>(GetTransientPackage());
		ShortDef->RetaliationPursuitSeconds = 1.25f;
		ShortRetaliationDefWeak = ShortDef;
		Victim->DebugApplyRetaliationPursuitSecondsFromDefinition(ShortDef);
		Expect(FMath::IsNearlyEqual(Victim->GetRetaliationPursuitSeconds(), 1.25f, 0.01f), TEXT("B_NonDefaultSeconds"));
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, Victim), TEXT("E_SameAttackerRefreshHit"));
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			RemainingAfterFirstHit = Cmd->DebugGetRetaliationRemainingSeconds();
			Expect(Cmd->IsRetaliationActive() && Cmd->GetRetaliationTarget() == Attacker, TEXT("E_StillSameAttacker"));
			Expect(!Cmd->HasHeldCommand(), TEXT("E_NoCommandThrash"));
			Expect(RemainingAfterFirstHit > 1.0f, TEXT("E_TimeoutRefreshed"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		AGP_SalvageWalker* AttackerB = AttackerBWeak.Get();
		if (!Expect(IsValid(Victim) && IsValid(AttackerB), TEXT("F_ActorsAlive")))
		{
			Finish();
			return;
		}
		Expect(GPCombatRetaliationDebug::ApplyHit(AttackerB, Victim), TEXT("F_LatestAttackerHit"));
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsRetaliationActive() && Cmd->GetRetaliationTarget() == AttackerB, TEXT("F_LatestAttackerOwns"));
			Expect(!Cmd->HasHeldCommand(), TEXT("F_StillNoHeldCommand"));
		}
		++StageIndex;
		ScheduleNext(1.40f);
		break;
	}
	case 3:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		if (!Expect(IsValid(Victim), TEXT("D_VictimAlive")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("D_TimeoutClearedRetaliation"));
			Expect(!Cmd->IsAttackActive(), TEXT("D_ReturnedIdleAttack"));
			Expect(!Cmd->HasHeldCommand(), TEXT("D_ReturnedIdleHeld"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		AGP_SalvageWalker* EngageVictim = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(0.0f, 2000.0f, 0.0f), TeamA);
		AGP_SalvageWalker* EngageAttacker = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(200.0f, 2000.0f, 0.0f), TeamB);
		EngageVictimWeak = EngageVictim;
		EngageAttackerWeak = EngageAttacker;
		if (!Expect(IsValid(EngageVictim) && IsValid(EngageAttacker), TEXT("C_SpawnInRange")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(EngageVictim, 400.0f, 25.0f, 400.0f, 1.0f);
		GPCombatRetaliationDebug::ApplyCombatStats(EngageAttacker, 400.0f, 15.0f, 250.0f, 1.0f);
		if (UGP_UnitCommandComponent* Cmd = EngageVictim->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireSightRangeCm = 900.0f;
		}
		Expect(GPCombatRetaliationDebug::ApplyHit(EngageAttacker, EngageVictim), TEXT("C_DamageApplied"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 5:
	{
		AGP_SalvageWalker* EngageVictim = EngageVictimWeak.Get();
		AGP_SalvageWalker* EngageAttacker = EngageAttackerWeak.Get();
		if (!Expect(IsValid(EngageVictim) && IsValid(EngageAttacker), TEXT("C_EngageAlive")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = EngageVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsAttackActive() && Cmd->GetAttackTarget() == EngageAttacker, TEXT("C_TransitionedToAttack"));
			Expect(!Cmd->IsRetaliationActive(), TEXT("C_RetaliationReleasedToAttackFSM"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		AGP_SalvageWalker* ManualVictim = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(0.0f, 4000.0f, 0.0f), TeamA);
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		ManualVictimWeak = ManualVictim;
		if (!Expect(IsValid(ManualVictim) && IsValid(Attacker), TEXT("G_ManualSpawn")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(ManualVictim, 400.0f, 25.0f, 250.0f, 1.0f);
		const FVector MoveDest = Origin + FVector(-800.0f, 4000.0f, 0.0f);
		GPCombatRetaliationDebug::IssueCommand(ManualVictim, GPTags.Command_Move, nullptr, MoveDest);
		if (UGP_UnitCommandComponent* Cmd = ManualVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
				TEXT("G_MoveHeldBeforeHit"));
			ManualMoveSerial = Cmd->GetHeldCommand()->CommandSerial;
		}
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, ManualVictim), TEXT("G_HitDuringMove"));
		if (UGP_UnitCommandComponent* Cmd = ManualVictim->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("G_MoveSuppressedRetaliation"));
			Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move
				&& Cmd->GetHeldCommand()->CommandSerial == ManualMoveSerial,
				TEXT("G_MoveUnchanged"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		if (!Expect(IsValid(Victim) && IsValid(Attacker), TEXT("H_ActorsAlive")))
		{
			Finish();
			return;
		}
		if (UGP_UnitDefinition* ShortDef = ShortRetaliationDefWeak.Get())
		{
			Victim->DebugApplyRetaliationPursuitSecondsFromDefinition(ShortDef);
		}
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, Victim), TEXT("H_RetaliationRestart"));
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsRetaliationActive(), TEXT("H_RetaliationOwnsBeforeManual"));
		}
		const FVector MoveDest = Origin + FVector(-600.0f, 0.0f, 0.0f);
		GPCombatRetaliationDebug::IssueCommand(Victim, GPTags.Command_Move, nullptr, MoveDest);
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("G_ManualCancelsRetaliation"));
			Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
				TEXT("H_ManualMoveHeld"));
			ManualMoveSerial = Cmd->GetHeldCommand()->CommandSerial;
		}
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 8:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		if (!Expect(IsValid(Victim), TEXT("H_VictimAfterWait")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = Victim->GetUnitCommandComponent())
		{
			Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move
				&& Cmd->GetHeldCommand()->CommandSerial == ManualMoveSerial,
				TEXT("H_StaleRetaliationDidNotReplaceManual"));
		}

		AGP_SalvageWalker* Friendly = GPCombatRetaliationDebug::SpawnSW(
			World, Origin + FVector(300.0f, -800.0f, 0.0f), TeamA);
		GPCombatRetaliationDebug::ApplyCombatStats(Friendly, 200.0f, 15.0f, 250.0f, 1.0f);
		FGP_DamageApplicationResult FriendlyResult;
		Expect(!Victim->ApplyDamageFromUnit(Friendly, FriendlyResult), TEXT("I_FriendlyRejected"));
		Expect(!Victim->ApplyDamageFromUnit(nullptr, FriendlyResult), TEXT("I_InvalidRejected"));
		if (AGP_SalvageWalker* DeadSource = AttackerBWeak.Get())
		{
			DeadSource->Destroy();
			AttackerBWeak.Reset();
		}
		FGP_DamageApplicationResult DeadResult;
		Expect(!Victim->ApplyDamageFromUnit(AttackerBWeak.Get(), DeadResult), TEXT("I_DeadInvalidRejected"));
		if (Friendly)
		{
			Friendly->Destroy();
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9:
	{
		AGP_Worker* Worker = GPCombatRetaliationDebug::SpawnWorker(
			World, Origin + FVector(0.0f, 6000.0f, 0.0f), TeamA);
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		WorkerVictimWeak = Worker;
		if (!Expect(IsValid(Worker) && IsValid(Attacker), TEXT("J_WorkerSpawn")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(Worker, 200.0f, 0.0f, 250.0f, 1.0f);
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, Worker), TEXT("J_WorkerDamaged"));
		if (UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("J_WorkerNoRetaliation"));
			Expect(!Cmd->IsAttackActive(), TEXT("J_WorkerNoAutonomousCombat"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10:
	{
		AGP_DefensiveTurret* Turret = GPCombatRetaliationDebug::SpawnTurret(
			World, Origin + FVector(800.0f, 6000.0f, 0.0f), TeamA);
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		TurretWeak = Turret;
		if (!Expect(IsValid(Turret) && IsValid(Attacker), TEXT("K_TurretSpawn")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(Turret, 400.0f, 20.0f, 600.0f, 1.0f);
		const FVector TurretLoc = Turret->GetActorLocation();
		Expect(GPCombatRetaliationDebug::ApplyHit(Attacker, Turret), TEXT("K_TurretDamaged"));
		if (UGP_UnitCommandComponent* Cmd = Turret->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("K_TurretNoRetaliation"));
		}
		if (UGP_MovementComponent* Movement = Turret->FindComponentByClass<UGP_MovementComponent>())
		{
			Expect(!Movement->IsMoving(), TEXT("K_TurretDidNotMove"));
		}
		Expect(FVector::Dist2D(Turret->GetActorLocation(), TurretLoc) < 5.0f, TEXT("K_TurretStationary"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11:
	{
		AGP_SalvageWalker* Victim = VictimWeak.Get();
		AGP_SalvageWalker* Attacker = AttackerWeak.Get();
		if (IsValid(Victim) && IsValid(Attacker))
		{
			if (UGP_UnitDefinition* ShortDef = ShortRetaliationDefWeak.Get())
			{
				Victim->DebugApplyRetaliationPursuitSecondsFromDefinition(ShortDef);
			}
			GPCombatRetaliationDebug::ApplyHit(Attacker, Victim);
		}
		if (IsValid(Victim))
		{
			Victim->Destroy();
			VictimWeak.Reset();
		}
		Expect(true, TEXT("L_OwnerDestroySafe"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12:
	{
		const FVector LOSOrigin = Origin + FVector(0.0f, 8000.0f, 0.0f);
		AGP_SalvageWalker* LOSVictim = GPCombatRetaliationDebug::SpawnSW(World, LOSOrigin, TeamA);
		AGP_SalvageWalker* LOSAttacker = GPCombatRetaliationDebug::SpawnSW(
			World, LOSOrigin + FVector(200.0f, 0.0f, 0.0f), TeamB);
		LOSVictimWeak = LOSVictim;
		LOSAttackerWeak = LOSAttacker;
		if (!Expect(IsValid(LOSVictim) && IsValid(LOSAttacker), TEXT("LOS_SpawnBlockedPair")))
		{
			Finish();
			return;
		}
		AActor* Blocker = GPCombatRetaliationDebug::SpawnVisibilityBlocker(
			World,
			(LOSVictim->GetActorLocation() + LOSAttacker->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 40.0f));
		LOSBlockerWeak = Blocker;
		if (!Expect(IsValid(Blocker), TEXT("LOS_A_SpawnBlocker")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(LOSVictim, 400.0f, 25.0f, 400.0f, 1.0f);
		GPCombatRetaliationDebug::ApplyCombatStats(LOSAttacker, 400.0f, 15.0f, 250.0f, 1.0f);
		if (UGP_UnitCommandComponent* Cmd = LOSVictim->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireSightRangeCm = 900.0f;
		}
		Expect(!GPCombatLOS::HasLineOfSight(World, LOSVictim, LOSAttacker), TEXT("LOS_A_HelperBlocked"));
		Expect(GPCombatRetaliationDebug::ApplyHit(LOSAttacker, LOSVictim), TEXT("LOS_A_DamageApplied"));
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 13:
	{
		AGP_SalvageWalker* LOSVictim = LOSVictimWeak.Get();
		AGP_SalvageWalker* LOSAttacker = LOSAttackerWeak.Get();
		if (!Expect(IsValid(LOSVictim) && IsValid(LOSAttacker), TEXT("LOS_A_ActorsAlive")))
		{
			Finish();
			return;
		}
		Expect(!GPCombatLOS::HasLineOfSight(World, LOSVictim, LOSAttacker), TEXT("LOS_A_StillBlocked"));
		if (UGP_UnitCommandComponent* Cmd = LOSVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsRetaliationActive() && Cmd->GetRetaliationTarget() == LOSAttacker, TEXT("LOS_A_RetaliationRemains"));
			Expect(!Cmd->IsAttackActive(), TEXT("LOS_A_NoAttackFSM"));
			Expect(!Cmd->HasHeldCommand(), TEXT("LOS_A_NoHeldAttack"));
		}
		if (AActor* Blocker = LOSBlockerWeak.Get())
		{
			Blocker->Destroy();
			LOSBlockerWeak.Reset();
		}
		++StageIndex;
		ScheduleNext(0.30f);
		break;
	}
	case 14:
	{
		AGP_SalvageWalker* LOSVictim = LOSVictimWeak.Get();
		AGP_SalvageWalker* LOSAttacker = LOSAttackerWeak.Get();
		if (!Expect(IsValid(LOSVictim) && IsValid(LOSAttacker), TEXT("LOS_B_ActorsAlive")))
		{
			Finish();
			return;
		}
		Expect(GPCombatLOS::HasLineOfSight(World, LOSVictim, LOSAttacker), TEXT("LOS_B_HelperClear"));
		if (UGP_UnitCommandComponent* Cmd = LOSVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsAttackActive() && Cmd->GetAttackTarget() == LOSAttacker, TEXT("LOS_B_HandoffToAttack"));
			Expect(!Cmd->IsRetaliationActive(), TEXT("LOS_B_RetaliationReleased"));
			AttackHandoffSerial = Cmd->GetActiveAttackSerial();
			Expect(AttackHandoffSerial != 0, TEXT("LOS_B_AttackSerialAssigned"));
		}
		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 15:
	{
		AGP_SalvageWalker* LOSVictim = LOSVictimWeak.Get();
		AGP_SalvageWalker* LOSAttacker = LOSAttackerWeak.Get();
		if (!Expect(IsValid(LOSVictim) && IsValid(LOSAttacker), TEXT("LOS_B_OnceAlive")))
		{
			Finish();
			return;
		}
		if (UGP_UnitCommandComponent* Cmd = LOSVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsAttackActive()
				&& Cmd->GetAttackTarget() == LOSAttacker
				&& Cmd->GetActiveAttackSerial() == AttackHandoffSerial,
				TEXT("LOS_B_HandoffExactlyOnce"));
			Expect(!Cmd->IsRetaliationActive(), TEXT("LOS_B_NoRetaliationRestart"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 16:
	{
		const FVector TimeoutOrigin = Origin + FVector(0.0f, 10000.0f, 0.0f);
		AGP_SalvageWalker* TimeoutVictim = GPCombatRetaliationDebug::SpawnSW(World, TimeoutOrigin, TeamA);
		AGP_SalvageWalker* TimeoutAttacker = GPCombatRetaliationDebug::SpawnSW(
			World, TimeoutOrigin + FVector(200.0f, 0.0f, 0.0f), TeamB);
		TimeoutLOSVictimWeak = TimeoutVictim;
		TimeoutLOSAttackerWeak = TimeoutAttacker;
		if (!Expect(IsValid(TimeoutVictim) && IsValid(TimeoutAttacker), TEXT("LOS_C_Spawn")))
		{
			Finish();
			return;
		}
		AActor* Blocker = GPCombatRetaliationDebug::SpawnVisibilityBlocker(
			World,
			(TimeoutVictim->GetActorLocation() + TimeoutAttacker->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 40.0f));
		TimeoutLOSBlockerWeak = Blocker;
		if (!Expect(IsValid(Blocker), TEXT("LOS_C_SpawnBlocker")))
		{
			Finish();
			return;
		}
		GPCombatRetaliationDebug::ApplyCombatStats(TimeoutVictim, 400.0f, 25.0f, 400.0f, 1.0f);
		GPCombatRetaliationDebug::ApplyCombatStats(TimeoutAttacker, 400.0f, 15.0f, 250.0f, 1.0f);
		if (UGP_UnitDefinition* ShortDef = ShortRetaliationDefWeak.Get())
		{
			TimeoutVictim->DebugApplyRetaliationPursuitSecondsFromDefinition(ShortDef);
		}
		else
		{
			UGP_UnitDefinition* ShortDefLocal = NewObject<UGP_UnitDefinition>(GetTransientPackage());
			ShortDefLocal->RetaliationPursuitSeconds = 1.25f;
			ShortRetaliationDefWeak = ShortDefLocal;
			TimeoutVictim->DebugApplyRetaliationPursuitSecondsFromDefinition(ShortDefLocal);
		}
		if (UGP_UnitCommandComponent* Cmd = TimeoutVictim->GetUnitCommandComponent())
		{
			Cmd->AutoAcquireSightRangeCm = 900.0f;
		}
		Expect(!GPCombatLOS::HasLineOfSight(World, TimeoutVictim, TimeoutAttacker), TEXT("LOS_C_HelperBlocked"));
		Expect(FMath::IsNearlyEqual(TimeoutVictim->GetRetaliationPursuitSeconds(), 1.25f, 0.01f), TEXT("LOS_C_ShortDuration"));
		Expect(GPCombatRetaliationDebug::ApplyHit(TimeoutAttacker, TimeoutVictim), TEXT("LOS_C_DamageApplied"));
		if (UGP_UnitCommandComponent* Cmd = TimeoutVictim->GetUnitCommandComponent())
		{
			Expect(Cmd->IsRetaliationActive(), TEXT("LOS_C_StartedRetaliation"));
			Expect(!Cmd->IsAttackActive() && !Cmd->HasHeldCommand(), TEXT("LOS_C_NoAttackAtStart"));
		}
		++StageIndex;
		ScheduleNext(1.40f);
		break;
	}
	case 17:
	{
		AGP_SalvageWalker* TimeoutVictim = TimeoutLOSVictimWeak.Get();
		if (!Expect(IsValid(TimeoutVictim), TEXT("LOS_C_VictimAlive")))
		{
			Finish();
			return;
		}
		Expect(!GPCombatLOS::HasLineOfSight(World, TimeoutVictim, TimeoutLOSAttackerWeak.Get()), TEXT("LOS_C_StillBlockedAtTimeout"));
		if (UGP_UnitCommandComponent* Cmd = TimeoutVictim->GetUnitCommandComponent())
		{
			Expect(!Cmd->IsRetaliationActive(), TEXT("LOS_C_TimeoutClearedRetaliation"));
			Expect(!Cmd->IsAttackActive(), TEXT("LOS_C_NoAttackAfterTimeout"));
			Expect(!Cmd->HasHeldCommand(), TEXT("LOS_C_NoHeldAttackAfterTimeout"));
		}
		if (UGP_MovementComponent* Movement = TimeoutVictim->FindComponentByClass<UGP_MovementComponent>())
		{
			Expect(!Movement->IsMoving(), TEXT("LOS_C_MovementStopped"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	default:
		Finish();
		break;
	}
}

#endif
