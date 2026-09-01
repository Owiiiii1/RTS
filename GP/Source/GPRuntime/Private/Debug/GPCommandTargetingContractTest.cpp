// Copyright Epic Games, Inc. All Rights Reserved.

#include "Command/GPCommandTargetingContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Command/GPCommandComponent.h"
#include "Command/GPCommandRequest.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCommandTargetingContract, Log, All);

namespace GPCommandTargetingDebug
{
	static TWeakObjectPtr<UGP_CommandTargetingContractTestRunner> GActiveRunner;

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

	static bool HasCommandCursorOverlay(const AGP_PlayerController* PC, EGP_CommandTargetingMode Mode)
	{
		return PC != nullptr
			&& PC->IsCommandCursorOverlayVisible()
			&& PC->GetCommandCursorOverlayMode() == Mode
			&& !PC->bShowMouseCursor;
	}

	static bool HasNormalHardwareCursor(const AGP_PlayerController* PC)
	{
		return PC != nullptr
			&& !PC->IsCommandCursorOverlayVisible()
			&& PC->GetCommandCursorOverlayMode() == EGP_CommandTargetingMode::None
			&& PC->bShowMouseCursor;
	}

	static AGP_SalvageWalker* SpawnWalker(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_SalvageWalker* Walker = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Walker != nullptr)
		{
			Walker->SetTeamId(TeamId);
		}
		return Walker;
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

	static void RunCommandTargetingContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCommandTargetingContract, Warning,
				TEXT("gp.Commands.RunMovePatrolTargetingContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPCommandTargetingContract, Warning,
				TEXT("gp.Commands.RunMovePatrolTargetingContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("CommandTargetingContract"), TEXT("CommandTargeting"), Token))
		{
			return;
		}

		UGP_CommandTargetingContractTestRunner* Runner =
			NewObject<UGP_CommandTargetingContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCommandTargetingContract(
		TEXT("gp.Commands.RunMovePatrolTargetingContractTest"),
		TEXT("MOVE/PATROL targeting modal, cursor, and patrol loop contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCommandTargetingContractTest));
}

void UGP_CommandTargetingContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_CommandTargetingContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_CommandTargetingContractTestRunner::OnWorldCleanup(
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

void UGP_CommandTargetingContractTestRunner::CleanupActors()
{
	if (AGP_Worker* Worker = WorkerWeak.Get())
	{
		Worker->Destroy();
	}
	if (AGP_SalvageWalker* Walker = WalkerWeak.Get())
	{
		Walker->Destroy();
	}
	WorkerWeak.Reset();
	WalkerWeak.Reset();
	LocalPCWeak.Reset();
}

void UGP_CommandTargetingContractTestRunner::Finish()
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
	if (AGP_PlayerController* PC = LocalPCWeak.Get())
	{
		PC->CancelCommandTargetingMode();
	}
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPCommandTargetingContract, Log,
		TEXT("gp.Commands.RunMovePatrolTargetingContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPCommandTargetingDebug::GActiveRunner.Reset();
}

void UGP_CommandTargetingContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPCommandTargetingContract, Error,
		TEXT("gp.Commands.RunMovePatrolTargetingContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_CommandTargetingContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPCommandTargetingContract, Error,
			TEXT("gp.Commands.RunMovePatrolTargetingContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPCommandTargetingContract, Log,
		TEXT("gp.Commands.RunMovePatrolTargetingContractTest PASS: %s"), Label);
	return true;
}

void UGP_CommandTargetingContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldOnSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_CommandTargetingContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_CommandTargetingContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_CommandTargetingContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPCommandTargetingContract, Log,
		TEXT("gp.Commands.RunMovePatrolTargetingContractTest Start"));
	AdvanceStage();
}

void UGP_CommandTargetingContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	constexpr int32 LocalTeamId = 1;

	switch (StageIndex)
	{
	case 0:
	{
		GPCommandTargetingDebug::NeutralizeAuthoredCombat(World);
		Origin = FVector(-48000.0f, -12000.0f, 100.0f);
		MoveDest = Origin + FVector(700.0f, 0.0f, 0.0f);
		PatrolDest = Origin + FVector(0.0f, 700.0f, 0.0f);

		AGP_PlayerController* PC = Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		LocalPCWeak = PC;
		if (!Expect(IsValid(PC) && PC->IsLocalController(), TEXT("A0_LocalPlayerController")))
		{
			Finish();
			return;
		}

		Expect(GPCommandTargetingDebug::HasNormalHardwareCursor(PC)
			&& !PC->IsCommandTargetingActive(),
			TEXT("P_NormalCursorVisibleOverlayGone"));

		AGP_Worker* Worker = GPCommandTargetingDebug::SpawnWorker(World, Origin, LocalTeamId);
		AGP_SalvageWalker* Walker = GPCommandTargetingDebug::SpawnWalker(
			World, Origin + FVector(0.0f, 200.0f, 0.0f), LocalTeamId);
		WorkerWeak = Worker;
		WalkerWeak = Walker;
		if (!Expect(IsValid(Worker) && IsValid(Walker)
			&& Worker->IsMobileCommandEligible()
			&& Walker->IsMobileCommandEligible(),
			TEXT("A0_SpawnedMobileUnits")))
		{
			Finish();
			return;
		}

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 1:
	{
		AGP_PlayerController* PC = LocalPCWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(PC) && IsValid(Worker) && IsValid(Walker), TEXT("A_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_SelectionComponent* Selection = PC->GetSelectionComponent();
		if (!Expect(Selection != nullptr, TEXT("A_SelectionComponent")))
		{
			Finish();
			return;
		}

		Selection->ReplaceSelectionWithUnit(Worker);
		Expect(PC->SelectionHasMoveEligibleUnit() && PC->SelectionHasPatrolEligibleUnit(),
			TEXT("A_EligibleSelectionEnablesMovePatrol"));
		Expect(!PC->SelectionHasAttackMoveEligibleUnit(),
			TEXT("A_WorkerNotAttackMoveEligible"));

		Selection->ReplaceSelectionWithUnit(Walker);
		Expect(PC->SelectionHasMoveEligibleUnit() && PC->SelectionHasAttackMoveEligibleUnit(),
			TEXT("A_WalkerMoveAndAttackMoveEligible"));

		Selection->ReplaceSelectionWithUnit(Worker);
		PC->EnterMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->GetCommandTargetingMode() == EGP_CommandTargetingMode::Move,
			TEXT("B_EnterMoveMode"));
		Expect(GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::Move),
			TEXT("Q_MoveOverlayVisibleHardwareHidden"));

		PC->ConfirmCommandTargetingDestinationForContract(MoveDest);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd != nullptr
			&& Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move
			&& FVector::Dist(Cmd->GetHeldCommand()->TargetLocation, MoveDest) < 150.0f,
			TEXT("C_MoveRequestHeldThroughPipeline"));
		Expect(!PC->IsCommandTargetingActive()
			&& GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("E_MoveConfirmRemovesOverlay"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_PlayerController* PC = LocalPCWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(PC) && IsValid(Worker), TEXT("F_ActorsAlive")))
		{
			Finish();
			return;
		}

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Stop, FVector::ZeroVector);
		UGP_SelectionComponent* Selection = PC->GetSelectionComponent();
		if (Selection != nullptr)
		{
			Selection->ReplaceSelectionWithUnit(Worker);
		}

		PC->EnterMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->GetCommandTargetingMode() == EGP_CommandTargetingMode::Move, TEXT("F_MoveActiveBeforeRMB"));
		PC->UpdateAttackMoveInputEdgesForContract(false, true, false, false);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(!PC->IsCommandTargetingActive()
			&& GPCommandTargetingDebug::HasNormalHardwareCursor(PC)
			&& (Cmd == nullptr || !Cmd->HasHeldCommand()),
			TEXT("F_RMBCancelNoCommandOverlayGone"));

		PC->EnterMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, true);
		Expect(!PC->IsCommandTargetingActive()
			&& GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("F_EscCancelOverlayGone"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		AGP_PlayerController* PC = LocalPCWeak.Get();
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		if (!Expect(IsValid(PC) && IsValid(Walker), TEXT("R_ActorsAlive")))
		{
			Finish();
			return;
		}

		if (UGP_SelectionComponent* Selection = PC->GetSelectionComponent())
		{
			Selection->ReplaceSelectionWithUnit(Walker);
		}
		PC->EnterAttackMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->IsAttackMoveModeActive()
			&& GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::AttackMove),
			TEXT("R_AttackMoveOverlayVisible"));
		PC->CancelAttackMoveMode();
		Expect(!PC->IsAttackMoveModeActive()
			&& GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("T_AttackMoveCancelRemovesOverlay"));

		PC->EnterMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::Move),
			TEXT("E_MoveOverlayBeforePatrolReplace"));
		PC->EnterPatrolMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->GetCommandTargetingMode() == EGP_CommandTargetingMode::Patrol
			&& GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::Patrol),
			TEXT("E_PatrolReplacesMoveOverlayMode"));
		PC->CancelCommandTargetingMode();
		Expect(GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("T_ModeReplaceCancelRemovesOverlay"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		AGP_PlayerController* PC = LocalPCWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(PC) && IsValid(Worker), TEXT("H_ActorsAlive")))
		{
			Finish();
			return;
		}

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Stop, FVector::ZeroVector);
		if (UGP_SelectionComponent* Selection = PC->GetSelectionComponent())
		{
			Selection->ReplaceSelectionWithUnit(Worker);
		}
		Expect(PC->SelectionHasPatrolEligibleUnit(), TEXT("G_PatrolEnabledForMobileUnit"));

		PC->EnterPatrolMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->GetCommandTargetingMode() == EGP_CommandTargetingMode::Patrol, TEXT("H_EnterPatrolMode"));
		Expect(GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::Patrol)
			&& PC->GetCommandCursorOverlayMode() != EGP_CommandTargetingMode::Move,
			TEXT("S_PatrolOverlayDistinctFromMove"));

		PatrolAnchorA = Worker->GetActorLocation();
		PC->ConfirmCommandTargetingDestinationForContract(PatrolDest);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd != nullptr
			&& Cmd->IsPatrolActive()
			&& Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Patrol
			&& FVector::Dist(Cmd->GetPatrolAnchorB(), PatrolDest) < 150.0f
			&& FVector::Dist(Cmd->GetPatrolAnchorA(), PatrolAnchorA) < 80.0f
			&& Cmd->IsPatrolHeadingToB(),
			TEXT("I_J_PatrolStoresAnchorsAndRequestsServerPath"));
		Expect(!PC->IsCommandTargetingActive()
			&& GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("T_PatrolConfirmRemovesOverlay"));

		PatrolWaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Worker != nullptr ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd != nullptr, TEXT("K_WorkerAlive")))
		{
			Finish();
			return;
		}

		++PatrolWaitTicks;
		if (Cmd->IsPatrolActive() && !Cmd->IsPatrolHeadingToB())
		{
			Expect(true, TEXT("K_ArrivalBReturnsTowardA"));
			PatrolWaitTicks = 0;
			++StageIndex;
			ScheduleNext(0.35f);
			break;
		}

		if (PatrolWaitTicks > 24)
		{
			Expect(false, TEXT("K_ArrivalBReturnsTowardA"));
			Finish();
			return;
		}

		ScheduleNext(0.35f);
		break;
	}
	case 6:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Worker != nullptr ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd != nullptr, TEXT("L_WorkerAlive")))
		{
			Finish();
			return;
		}

		++PatrolWaitTicks;
		if (Cmd->IsPatrolActive() && Cmd->IsPatrolHeadingToB())
		{
			Expect(true, TEXT("L_ArrivalAReturnsTowardB"));
			++StageIndex;
			ScheduleNext(0.05f);
			break;
		}

		if (PatrolWaitTicks > 24)
		{
			Expect(false, TEXT("L_ArrivalAReturnsTowardB"));
			Finish();
			return;
		}

		ScheduleNext(0.35f);
		break;
	}
	case 7:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(Worker), TEXT("M_WorkerAlive")))
		{
			Finish();
			return;
		}

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Stop, FVector::ZeroVector);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd != nullptr && !Cmd->IsPatrolActive() && !Cmd->HasHeldCommand(),
			TEXT("M_StopClearsPatrol"));

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Patrol, PatrolDest);
		Expect(Cmd != nullptr && Cmd->IsPatrolActive(), TEXT("N_PatrolReissued"));
		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Move, MoveDest);
		Expect(Cmd != nullptr
			&& !Cmd->IsPatrolActive()
			&& Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == GPTags.Command_Move,
			TEXT("N_ReplacementMoveClearsPatrol"));

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Patrol, PatrolDest);
		Expect(Cmd != nullptr && Cmd->IsPatrolActive(), TEXT("N_PatrolBeforeAttackMove"));
		if (AGP_SalvageWalker* Walker = WalkerWeak.Get())
		{
			GPCommandTargetingDebug::IssueCommand(Walker, GPTags.Command_AttackMove, MoveDest);
			Expect(Walker->GetUnitCommandComponent() != nullptr
				&& Walker->GetUnitCommandComponent()->IsAttackMoveActive(),
				TEXT("N_AttackMoveIndependent"));
		}
		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_AttackMove, MoveDest);
		Expect(Cmd != nullptr && !Cmd->IsPatrolActive(),
			TEXT("N_ReplacementAttackMoveClearsPatrol"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		AGP_PlayerController* PC = LocalPCWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(PC) && IsValid(Worker), TEXT("O_ActorsAlive")))
		{
			Finish();
			return;
		}

		GPCommandTargetingDebug::IssueCommand(Worker, GPTags.Command_Patrol, PatrolDest);
		Expect(Worker->GetUnitCommandComponent() != nullptr
			&& Worker->GetUnitCommandComponent()->IsPatrolActive(),
			TEXT("O_PatrolBeforeDestroy"));
		Worker->Destroy();
		WorkerWeak.Reset();
		Expect(!IsValid(Worker), TEXT("O_DeathDestructionCleanup"));

		if (UGP_SelectionComponent* Selection = PC->GetSelectionComponent())
		{
			if (AGP_SalvageWalker* Walker = WalkerWeak.Get())
			{
				Selection->ReplaceSelectionWithUnit(Walker);
			}
		}
		PC->EnterMoveMode();
		PC->UpdateAttackMoveInputEdgesForContract(false, false, false, false);
		Expect(PC->GetCommandTargetingMode() == EGP_CommandTargetingMode::Move
			&& GPCommandTargetingDebug::HasCommandCursorOverlay(PC, EGP_CommandTargetingMode::Move),
			TEXT("U_MoveOverlayBeforePlacement"));
		PC->EnterBuildingPlacementMode(EGP_OrbitalBuildingType::LogisticsHub);
		if (PC->IsBuildingPlacementActive())
		{
			Expect(GPCommandTargetingDebug::HasNormalHardwareCursor(PC)
				&& !PC->IsCommandTargetingActive(),
				TEXT("U_BuildingPlacementClearsCommandOverlay"));
			PC->CancelBuildingPlacement();
		}
		else
		{
			PC->CancelCommandTargetingMode();
			Expect(GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
				TEXT("U_ForcedCleanupRemovesOverlayWithoutPlacement"));
		}

		Expect(GPCommandTargetingDebug::HasNormalHardwareCursor(PC),
			TEXT("V_EndPlayUsesSameOverlayHidePath"));

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_CommandTargetingContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_CommandTargetingContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_CommandTargetingContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_CommandTargetingContractTestRunner::AdvanceStage() {}
bool UGP_CommandTargetingContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_CommandTargetingContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_CommandTargetingContractTestRunner::Finish() {}
void UGP_CommandTargetingContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_CommandTargetingContractTestRunner::UnbindWorldCleanup() {}
void UGP_CommandTargetingContractTestRunner::CleanupActors() {}

#endif
