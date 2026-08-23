// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPUnitCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPWorkerCommandIntent, Log, All);

namespace GPWorkerCommandIntentDebug
{
	static TWeakObjectPtr<UGP_WorkerCommandIntentContractTestRunner> GActiveRunner;

	static void IssueMine(AGP_Worker* Worker, AGP_ResourceNode* Node)
	{
		if (!IsValid(Worker) || !IsValid(Node))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Mine;
		Command.TargetActor = Node;
		Command.TargetLocation = Node->GetActorLocation();
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void IssueMove(AGP_Worker* Worker, const FVector& Location)
	{
		if (!IsValid(Worker))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Move;
		Command.TargetActor = nullptr;
		Command.TargetLocation = Location;
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void IssueMainBase(AGP_Worker* Worker, AGP_MainBase* MainBase)
	{
		if (!IsValid(Worker) || !IsValid(MainBase))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Move;
		Command.TargetActor = MainBase;
		Command.TargetLocation = MainBase->GetActorLocation();
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void IssueStop(AGP_Worker* Worker)
	{
		if (!IsValid(Worker))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Stop;
		Command.TargetActor = nullptr;
		Command.TargetLocation = FVector::ZeroVector;
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void FillCargo(AGP_Worker* Worker)
	{
		if (!IsValid(Worker) || Worker->GetCargoComponent() == nullptr)
		{
			return;
		}
		UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
		Cargo->ClearCargo();
		Cargo->AddCargo(Cargo->GetCargoCapacity());
	}

	static void PlaceAtDropOff(AGP_Worker* Worker, const FVector& DropOffLocation)
	{
		if (!IsValid(Worker))
		{
			return;
		}
		Worker->SetActorLocation(DropOffLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	static bool IsHeldMineOn(UGP_UnitCommandComponent* Cmd, const AGP_ResourceNode* Node)
	{
		const FGP_StoredUnitCommand* Held = Cmd != nullptr ? Cmd->GetHeldCommand() : nullptr;
		return Cmd != nullptr
			&& Cmd->HasHeldCommand()
			&& Held != nullptr
			&& Held->CommandTag == FGPGameplayTags::Get().Command_Mine
			&& Held->TargetActor.Get() == Node;
	}

	static bool IsLatentOrchestrationCleared(UGP_UnitCommandComponent* Cmd)
	{
		return Cmd != nullptr
			&& Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::Idle
			&& Cmd->GetActiveHaulSerial() == 0
			&& !Cmd->ShouldReturnToDepositAfterHaul()
			&& !Cmd->DebugIsWaitingRegisterBound()
			&& !Cmd->DebugIsDropOffStorageWakeBound()
			&& !Cmd->DebugIsActiveHaulUnregisterBound()
			&& !Cmd->DebugIsDropOffRetryArmed();
	}

	static bool IsHaulOrWaitActive(UGP_UnitCommandComponent* Cmd)
	{
		if (Cmd == nullptr)
		{
			return false;
		}
		const EGP_HaulExecutionState Haul = Cmd->GetHaulExecutionState();
		return Cmd->IsHaulActive()
			|| Haul == EGP_HaulExecutionState::ReturningToBase
			|| Haul == EGP_HaulExecutionState::DroppingOff
			|| Haul == EGP_HaulExecutionState::WaitingForDropOff;
	}

	static void FillStorage(AGP_MainBase* Base)
	{
		UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
		if (Storage == nullptr)
		{
			return;
		}
		Storage->AddPlanetaryFerronite(Storage->GetTotalCapacity());
	}

	static void FreeStorage(AGP_MainBase* Base)
	{
		UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
		if (Storage == nullptr)
		{
			return;
		}
		Storage->RemovePlanetaryFerronite(Storage->GetTotalCapacity());
	}

	static void RunWorkerCommandIntentContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorkerCommandIntent, Warning,
				TEXT("gp.Worker.RunCommandIntentContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("WorkerCommandIntentContract"), TEXT("WorkerCommandIntent"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_WorkerCommandIntentContractTestRunner* Runner =
			NewObject<UGP_WorkerCommandIntentContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWorkerCommandIntentContract(
		TEXT("gp.Worker.RunCommandIntentContractTest"),
		TEXT("Authority: Mine-cycle vs one-shot MainBase deposit command-intent contract. Transient only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunWorkerCommandIntentContractTest));
}

void UGP_WorkerCommandIntentContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_WorkerCommandIntentContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_WorkerCommandIntentContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
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

void UGP_WorkerCommandIntentContractTestRunner::CleanupActors()
{
	auto DestroyWeak = [](auto& Weak)
	{
		if (Weak.IsValid())
		{
			Weak->Destroy();
			Weak.Reset();
		}
	};
	DestroyWeak(WorkerWeak);
	DestroyWeak(NodeAWeak);
	DestroyWeak(NodeBWeak);
	DestroyWeak(MainBaseWeak);
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
}

void UGP_WorkerCommandIntentContractTestRunner::Finish()
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

	UE_LOG(LogGPWorkerCommandIntent, Log,
		TEXT("gp.Worker.RunCommandIntentContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPWorkerCommandIntentDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_WorkerCommandIntentContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPWorkerCommandIntent, Error,
		TEXT("gp.Worker.RunCommandIntentContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_WorkerCommandIntentContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPWorkerCommandIntent, Error,
			TEXT("gp.Worker.RunCommandIntentContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPWorkerCommandIntent, Log,
			TEXT("gp.Worker.RunCommandIntentContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_WorkerCommandIntentContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr || bFinished)
	{
		Finish();
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_WorkerCommandIntentContractTestRunner::AdvanceStage),
		FMath::Max(DelaySeconds, 0.01f),
		false);
}

bool UGP_WorkerCommandIntentContractTestRunner::WaitUntil(bool bDone, const TCHAR* TimeoutLabel, float TimeoutSeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("WaitMissingWorld"));
		return true;
	}
	if (bDone)
	{
		WaitTicks = 0;
		WaitStartTime = -1.0;
		return false;
	}
	if (WaitStartTime < 0.0)
	{
		WaitStartTime = World->GetTimeSeconds();
	}
	++WaitTicks;
	if ((World->GetTimeSeconds() - WaitStartTime) > TimeoutSeconds)
	{
		Expect(false, TimeoutLabel);
		Finish();
		return true;
	}
	ScheduleNext(0.05f);
	return true;
}

void UGP_WorkerCommandIntentContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	WaitTicks = 0;
	WaitStartTime = -1.0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_WorkerCommandIntentContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWorkerCommandIntent, Log, TEXT("gp.Worker.RunCommandIntentContractTest Start"));
	ScheduleNext(0.05f);
}

void UGP_WorkerCommandIntentContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	using namespace GPWorkerCommandIntentDebug;

	switch (StageIndex)
	{
	case 0:
	{
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("SpawnNavigableScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		MainBaseWeak = Scenario.MainBase;
		NodeAWeak = Scenario.ResourceNode;
		ContractTeamId = Scenario.TeamId;
		BaseDropOffLocation = Scenario.BaseDropOffLocation;
		NodeALocation = Scenario.ResourceNodeLocation;
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		// A: full cargo + explicit Mine(A) is accepted and hauls, then returns to A.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(NodeA) && Cmd, TEXT("CaseAObjects")))
		{
			Finish();
			return;
		}
		FreeStorage(Base);
		PlaceAtDropOff(Worker, BaseDropOffLocation);
		FillCargo(Worker);
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CaseACargoFull"));
		IssueMine(Worker, NodeA);
		Expect(IsHeldMineOn(Cmd, NodeA), TEXT("CaseAMineAcceptedHeldA"));
		Expect(Cmd->ShouldReturnToDepositAfterHaul(), TEXT("CaseAReturnToDepositArmed"));
		Expect(IsHaulOrWaitActive(Cmd), TEXT("CaseAHaulStarted"));
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && Cmd, TEXT("CaseAWaitObjects")))
		{
			Finish();
			return;
		}
		const bool bReturnedToA =
			Worker->GetCargoComponent()->IsEmpty()
			&& (IsHeldMineOn(Cmd, NodeA)
				&& (Cmd->GetMineTarget() == NodeA
					|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching
					|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Active
					|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit));
		if (WaitUntil(bReturnedToA, TEXT("CaseAReturnToA_Timeout")))
		{
			return;
		}
		Expect(IsHeldMineOn(Cmd, NodeA), TEXT("CaseAStillAssignedA"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		// B: explicit MainBase breaks mining cycle after unload.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(NodeA) && Cmd, TEXT("CaseBObjects")))
		{
			Finish();
			return;
		}
		IssueStop(Worker);
		FreeStorage(Base);
		PlaceAtDropOff(Worker, BaseDropOffLocation);
		FillCargo(Worker);
		IssueMine(Worker, NodeA);
		Expect(IsHeldMineOn(Cmd, NodeA), TEXT("CaseBMineAHeld"));
		IssueMainBase(Worker, Base);
		const FGP_StoredUnitCommand* Held = Cmd->GetHeldCommand();
		const bool bOneShotMoveHeld =
			Held != nullptr
			&& Held->CommandTag == FGPGameplayTags::Get().Command_Move
			&& Held->TargetActor.Get() == Base;
		const bool bAlreadyUnloadedIdle =
			Worker->GetCargoComponent()->IsEmpty()
			&& !IsHeldMineOn(Cmd, NodeA)
			&& Cmd->GetMineTarget() == nullptr;
		Expect(bOneShotMoveHeld || bAlreadyUnloadedIdle, TEXT("CaseBOneShotDepositOrUnloaded"));
		Expect(!Cmd->ShouldReturnToDepositAfterHaul(), TEXT("CaseBNoReturnToDeposit"));
		Expect(IsHaulOrWaitActive(Cmd) || Worker->GetCargoComponent()->IsEmpty(), TEXT("CaseBDepositStarted"));
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && Cmd, TEXT("CaseBWaitObjects")))
		{
			Finish();
			return;
		}
		if (WaitUntil(Worker->GetCargoComponent()->IsEmpty(), TEXT("CaseBUnload_Timeout")))
		{
			return;
		}
		Expect(!IsHeldMineOn(Cmd, NodeA), TEXT("CaseBMineANotRestored"));
		Expect(Cmd->GetMineTarget() == nullptr, TEXT("CaseBMineTargetCleared"));
		Expect(!Cmd->ShouldReturnToDepositAfterHaul(), TEXT("CaseBReturnFlagCleared"));
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.25f);
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && Cmd, TEXT("CaseBLingerObjects")))
		{
			Finish();
			return;
		}
		Expect(!IsHeldMineOn(Cmd, NodeA), TEXT("CaseBMineAStillDead"));
		Expect(Cmd->GetMineTarget() == nullptr, TEXT("CaseBNoLateMineTarget"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		// C: WaitingForDropOff replaced by Move — later storage space must not resume haul.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(NodeA) && Cmd, TEXT("CaseCObjects")))
		{
			Finish();
			return;
		}
		IssueStop(Worker);
		FillStorage(Base);
		PlaceAtDropOff(Worker, BaseDropOffLocation);
		FillCargo(Worker);
		IssueMine(Worker, NodeA);
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("CaseCWaiting"));
		const FVector MoveDest = Worker->GetActorLocation() + FVector(0.0f, 1200.0f, 0.0f);
		IssueMove(Worker, MoveDest);
		Expect(IsLatentOrchestrationCleared(Cmd), TEXT("CaseCLatentCleared"));
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CaseCCargoKept"));
		FreeStorage(Base);
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.25f);
		break;
	}
	case 7:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd, TEXT("CaseCWakeObjects")))
		{
			Finish();
			return;
		}
		Expect(IsLatentOrchestrationCleared(Cmd), TEXT("CaseCNoHaulResurrect"));
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CaseCCargoStillHeld"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		// D: WaitingForDropOff replaced by Mine(B) — haul then return to B, not A.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(NodeA), TEXT("CaseDObjects")))
		{
			Finish();
			return;
		}
		FString SpawnFail;
		AGP_ResourceNode* NodeB = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, NodeALocation + FVector(400.0f, 0.0f, 0.0f), OwnerTag);
		if (!IsValid(NodeB))
		{
			FVector Projected;
			if (GPResourceLoopDiagnostics::IsNavPointProjected(
					World, NodeALocation + FVector(0.0f, 450.0f, 0.0f), &Projected))
			{
				NodeB = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(World, Projected, OwnerTag);
			}
		}
		if (!Expect(IsValid(NodeB), TEXT("CaseDSpawnNodeB")))
		{
			Finish();
			return;
		}
		NodeBWeak = NodeB;
		IssueStop(Worker);
		FillStorage(Base);
		PlaceAtDropOff(Worker, BaseDropOffLocation);
		FillCargo(Worker);
		IssueMine(Worker, NodeA);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("CaseDWaiting"));
		IssueMine(Worker, NodeB);
		Expect(IsHeldMineOn(Cmd, NodeB), TEXT("CaseDHeldMineB"));
		Expect(Cmd->ShouldReturnToDepositAfterHaul(), TEXT("CaseDReturnArmedForB"));
		Expect(IsHaulOrWaitActive(Cmd), TEXT("CaseDHaulForB"));
		FreeStorage(Base);
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.05f);
		break;
	}
	case 9:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && IsValid(NodeB) && Cmd, TEXT("CaseDWaitObjects")))
		{
			Finish();
			return;
		}
		const bool bReturnedToB =
			Worker->GetCargoComponent()->IsEmpty()
			&& IsHeldMineOn(Cmd, NodeB)
			&& (Cmd->GetMineTarget() == NodeB
				|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching
				|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Active
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit);
		if (WaitUntil(bReturnedToB, TEXT("CaseDReturnToB_Timeout")))
		{
			return;
		}
		Expect(!IsHeldMineOn(Cmd, NodeA), TEXT("CaseDDidNotReturnToA"));
		Expect(IsHeldMineOn(Cmd, NodeB), TEXT("CaseDOwnsB"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10:
	{
		// E: Stop replacement clears latent haul/wait and does not resume.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(NodeA) && Cmd, TEXT("CaseEObjects")))
		{
			Finish();
			return;
		}
		IssueStop(Worker);
		FillStorage(Base);
		PlaceAtDropOff(Worker, BaseDropOffLocation);
		FillCargo(Worker);
		IssueMine(Worker, NodeA);
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff
				|| IsHaulOrWaitActive(Cmd),
			TEXT("CaseEHaulOrWait"));
		IssueStop(Worker);
		Expect(!Cmd->HasHeldCommand(), TEXT("CaseEHeldCleared"));
		Expect(IsLatentOrchestrationCleared(Cmd), TEXT("CaseELatentCleared"));
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CaseECargoKept"));
		FreeStorage(Base);
		++StageIndex;
		WaitTicks = 0;
		WaitStartTime = -1.0;
		ScheduleNext(0.25f);
		break;
	}
	case 11:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd, TEXT("CaseEWakeObjects")))
		{
			Finish();
			return;
		}
		Expect(!Cmd->HasHeldCommand(), TEXT("CaseEStillIdleHeld"));
		Expect(IsLatentOrchestrationCleared(Cmd), TEXT("CaseENoResume"));
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CaseECargoUnchanged"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12:
		Finish();
		break;
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_WorkerCommandIntentContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_WorkerCommandIntentContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_WorkerCommandIntentContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_WorkerCommandIntentContractTestRunner::AdvanceStage() {}
bool UGP_WorkerCommandIntentContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_WorkerCommandIntentContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_WorkerCommandIntentContractTestRunner::Finish() {}
void UGP_WorkerCommandIntentContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_WorkerCommandIntentContractTestRunner::UnbindWorldCleanup() {}
void UGP_WorkerCommandIntentContractTestRunner::CleanupActors() {}
bool UGP_WorkerCommandIntentContractTestRunner::WaitUntil(bool bDone, const TCHAR* TimeoutLabel, float TimeoutSeconds)
{
	(void)bDone;
	(void)TimeoutLabel;
	(void)TimeoutSeconds;
	return false;
}

#endif
