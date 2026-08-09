// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPMineReassignmentHaulContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMineReassignmentHaul, Log, All);

namespace GPMineReassignmentHaulDebug
{
	static TWeakObjectPtr<UGP_MineReassignmentHaulContractTestRunner> GActiveRunner;

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

	static void ApplySearchSettings(float SearchRadiusCm, float MaxPathLengthCm)
	{
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			Settings->ResourceSearchRadiusCm = SearchRadiusCm;
			Settings->MaxResourcePathLengthCm = MaxPathLengthCm;
		}
	}

	static AGP_ResourceNode* SpawnNodeBNearA(
		UWorld* World,
		FName OwnerTag,
		const FVector& NodeALoc,
		float SearchRadiusCm,
		FString& OutFailReason)
	{
		OutFailReason.Reset();
		if (!IsValid(World))
		{
			OutFailReason = TEXT("MissingWorld");
			return nullptr;
		}

		const FVector Offsets[] = {
			FVector(400.0f, 0.0f, 0.0f),
			FVector(-400.0f, 0.0f, 0.0f),
			FVector(0.0f, 400.0f, 0.0f),
			FVector(0.0f, -400.0f, 0.0f),
			FVector(500.0f, 200.0f, 0.0f),
			FVector(-500.0f, -200.0f, 0.0f),
			FVector(300.0f, 300.0f, 0.0f),
			FVector(-300.0f, 300.0f, 0.0f),
		};

		for (const FVector& Offset : Offsets)
		{
			FVector Projected;
			if (!GPResourceLoopDiagnostics::IsNavPointProjected(
					World, NodeALoc + Offset, &Projected, 800.0f, 800.0f))
			{
				continue;
			}
			if (FVector::Dist(Projected, NodeALoc) > SearchRadiusCm)
			{
				continue;
			}

			AGP_ResourceNode* Node = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
				World, Projected, OwnerTag);
			if (IsValid(Node))
			{
				return Node;
			}
		}

		OutFailReason = TEXT("NoProjectedNodeBNearA");
		return nullptr;
	}

	static bool FillNodeAOccupancy(
		UWorld* World,
		AGP_ResourceNode* NodeA,
		int32 TeamId,
		FName OwnerTag,
		TArray<TWeakObjectPtr<AGP_Worker>>& OutFillers)
	{
		if (!IsValid(World) || !IsValid(NodeA))
		{
			return false;
		}

		const int32 MaxSlots = NodeA->GetMaxConcurrentMiners();
		const FVector NodeLoc = NodeA->GetActorLocation();
		for (int32 i = 0; i < MaxSlots; ++i)
		{
			const FVector Offset(40.0f * static_cast<float>(i % 2), 40.0f * static_cast<float>(i / 2), 0.0f);
			AGP_Worker* Filler = GPResourceLoopDiagnostics::SpawnWorkerDeferred(
				World, NodeLoc + Offset, TeamId, OwnerTag);
			if (!IsValid(Filler) || !IsValid(Filler->GetMiningComponent()))
			{
				return false;
			}
			Filler->SetActorLocation(NodeLoc + Offset, false, nullptr, ETeleportType::TeleportPhysics);
			const EGP_BeginMiningResult BeginResult = Filler->GetMiningComponent()->BeginMining(NodeA);
			if (BeginResult != EGP_BeginMiningResult::Started
				&& BeginResult != EGP_BeginMiningResult::AlreadyMiningTarget)
			{
				// Slot request path as fallback if BeginMining needs occupancy first.
				const EGP_MiningSlotRequestResult SlotResult = NodeA->RequestMiningSlot(Filler);
				if (SlotResult != EGP_MiningSlotRequestResult::Granted
					&& SlotResult != EGP_MiningSlotRequestResult::AlreadyActive)
				{
					return false;
				}
			}
			OutFillers.Add(Filler);
		}

		return NodeA->GetActiveMinerCount() >= MaxSlots;
	}

	static bool IsAssignedToNode(AGP_Worker* Worker, AGP_ResourceNode* Node)
	{
		if (!IsValid(Worker) || !IsValid(Node))
		{
			return false;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (Cmd == nullptr)
		{
			return false;
		}
		if (Cmd->GetMineTarget() == Node)
		{
			return true;
		}
		if (Cmd->HasHeldCommand() && Cmd->GetHeldCommand()->TargetActor.Get() == Node)
		{
			return true;
		}
		if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
		{
			if (Mining->GetCurrentResourceNode() == Node)
			{
				return true;
			}
		}
		const EGP_WorkerActivityState Activity = Worker->GetWorkerActivityState();
		return (Activity == EGP_WorkerActivityState::MovingToMine
				|| Activity == EGP_WorkerActivityState::Mining)
			&& Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->TargetActor.Get() == Node;
	}

	static bool HasOwnedLogisticsHub(UWorld* World, FName OwnerTag)
	{
		if (!IsValid(World) || OwnerTag.IsNone())
		{
			return false;
		}
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (GPResourceLoopDiagnostics::ActorHasOwnerTag(*It, OwnerTag))
			{
				return true;
			}
		}
		return false;
	}

	static void RunMineReassignmentHaulContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMineReassignmentHaul, Warning,
				TEXT("gp.Resource.RunMineReassignmentHaulContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("MineReassignmentHaulContract"), TEXT("MineReassignmentHaul"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_MineReassignmentHaulContractTestRunner* Runner =
			NewObject<UGP_MineReassignmentHaulContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMineReassignmentHaulContract(
		TEXT("gp.Resource.RunMineReassignmentHaulContractTest"),
		TEXT("Authority: GP-S33M SlotFullAlternative → CargoFull haul → unload → return → MineRejected CargoFull. Transient only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunMineReassignmentHaulContractTest));
}

void UGP_MineReassignmentHaulContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_MineReassignmentHaulContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_MineReassignmentHaulContractTestRunner::OnWorldCleanup(
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

void UGP_MineReassignmentHaulContractTestRunner::CleanupActors()
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
	for (TWeakObjectPtr<AGP_Worker>& Weak : FillerWorkers)
	{
		DestroyWeak(Weak);
	}
	FillerWorkers.Reset();
	DestroyWeak(NodeAWeak);
	DestroyWeak(NodeBWeak);
	DestroyWeak(MainBaseWeak);
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
}

void UGP_MineReassignmentHaulContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (bSettingsOverridden)
	{
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			Settings->ResourceSearchRadiusCm = SavedSettingsSearchRadiusCm;
			Settings->MaxResourcePathLengthCm = SavedSettingsMaxPathLengthCm;
		}
		bSettingsOverridden = false;
	}

	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	UnbindWorldCleanup();
	CleanupActors();

	UE_LOG(LogGPMineReassignmentHaul, Log,
		TEXT("gp.Resource.RunMineReassignmentHaulContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPMineReassignmentHaulDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_MineReassignmentHaulContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPMineReassignmentHaul, Error,
		TEXT("gp.Resource.RunMineReassignmentHaulContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_MineReassignmentHaulContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPMineReassignmentHaul, Error,
			TEXT("gp.Resource.RunMineReassignmentHaulContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPMineReassignmentHaul, Log,
			TEXT("gp.Resource.RunMineReassignmentHaulContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_MineReassignmentHaulContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	if (DelaySeconds <= KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UGP_MineReassignmentHaulContractTestRunner::AdvanceStage));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_MineReassignmentHaulContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_MineReassignmentHaulContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	MovementWaitTicks = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_MineReassignmentHaulContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMineReassignmentHaul, Log,
		TEXT("gp.Resource.RunMineReassignmentHaulContractTest Start"));
	ScheduleNext(0.0f);
}

void UGP_MineReassignmentHaulContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	using namespace GPMineReassignmentHaulDebug;

	auto WaitCondition = [&](bool bDone, const TCHAR* TimeoutLabel) -> bool
	{
		if (bDone)
		{
			return false;
		}
		if (MovementWaitTicks == 0)
		{
			MovementWaitStartTime = World->GetTimeSeconds();
		}
		++MovementWaitTicks;
		if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
		{
			Expect(false, TimeoutLabel);
			Finish();
			return true;
		}
		ScheduleNext(0.1f);
		return true;
	};

	switch (StageIndex)
	{
	case 0:
	{
		// A — spawn MainBase + Worker + NodeA (+ NodeB), fill NodeA occupancy. Building out of scope.
		constexpr float TestSearchRadiusCm = 1500.0f;
		constexpr float TestMaxPathLengthCm = 8000.0f;
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			SavedSettingsSearchRadiusCm = Settings->ResourceSearchRadiusCm;
			SavedSettingsMaxPathLengthCm = Settings->MaxResourcePathLengthCm;
			bSettingsOverridden = true;
		}
		ApplySearchSettings(TestSearchRadiusCm, TestMaxPathLengthCm);

		GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Scenario.bOk || !Scenario.bReadyForHaulingTest)
		{
			UE_LOG(LogGPMineReassignmentHaul, Warning,
				TEXT("gp.Resource.RunMineReassignmentHaulContractTest: SpawnDiagnosticScenario failed (%s) — arena fallback"),
				*Scenario.Error);

			FVector Anchor = FVector(0.0f, 0.0f, 100.0f);
			FVector Projected;
			if (GPResourceLoopDiagnostics::IsNavPointProjected(World, Anchor, &Projected, 2000.0f, 2000.0f))
			{
				Anchor = Projected;
			}
			const int32 FreeTeam = GPResourceLoopDiagnostics::FindFreePlayableTeamId(World);
			ContractTeamId = FreeTeam >= 1 ? FreeTeam : 1;
			AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
				World, Anchor + FVector(-800.0f, 0.0f, 0.0f), ContractTeamId, OwnerTag);
			AGP_ResourceNode* NodeA = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
				World, Anchor + FVector(400.0f, 0.0f, 0.0f), OwnerTag);
			AGP_Worker* Worker = GPResourceLoopDiagnostics::SpawnWorkerDeferred(
				World, Anchor + FVector(280.0f, 0.0f, 0.0f), ContractTeamId, OwnerTag);
			Scenario.bOk = IsValid(Base) && IsValid(NodeA) && IsValid(Worker);
			Scenario.bReadyForHaulingTest = Scenario.bOk;
			Scenario.MainBase = Base;
			Scenario.ResourceNode = NodeA;
			Scenario.Worker = Worker;
			Scenario.TeamId = ContractTeamId;
			Scenario.MainBaseLocation = IsValid(Base) ? Base->GetActorLocation() : Anchor;
			Scenario.ResourceNodeLocation = IsValid(NodeA) ? NodeA->GetActorLocation() : Anchor;
		}

		if (!Expect(Scenario.bOk && IsValid(Scenario.MainBase) && IsValid(Scenario.Worker)
				&& IsValid(Scenario.ResourceNode),
				TEXT("SpawnMainBaseWorkerNodeA")))
		{
			Finish();
			return;
		}

		MainBaseWeak = Scenario.MainBase;
		WorkerWeak = Scenario.Worker;
		NodeAWeak = Scenario.ResourceNode;
		ContractTeamId = Scenario.TeamId;
		MainBaseLocation = Scenario.MainBase->GetActorLocation();
		NodeALocation = Scenario.ResourceNode->GetActorLocation();

		Expect(!HasOwnedLogisticsHub(World, OwnerTag), TEXT("BuildingOutOfScope_NoOwnedLogisticsHub"));

		FString NodeBFail;
		AGP_ResourceNode* NodeB = SpawnNodeBNearA(
			World, OwnerTag, NodeALocation, TestSearchRadiusCm, NodeBFail);
		if (!Expect(IsValid(NodeB), TEXT("SpawnNodeBNearA")))
		{
			UE_LOG(LogGPMineReassignmentHaul, Error,
				TEXT("gp.Resource.RunMineReassignmentHaulContractTest NodeB FAIL Reason=%s"),
				*NodeBFail);
			Finish();
			return;
		}
		NodeBWeak = NodeB;
		NodeBLocation = NodeB->GetActorLocation();
		NodeB->DebugSetCurrentAmountForTest(5000, false);
		Scenario.ResourceNode->DebugSetCurrentAmountForTest(5000, false);

		if (!Expect(FillNodeAOccupancy(World, Scenario.ResourceNode, ContractTeamId, OwnerTag, FillerWorkers),
				TEXT("FillNodeAOccupancy")))
		{
			UE_LOG(LogGPMineReassignmentHaul, Error,
				TEXT("gp.Resource.RunMineReassignmentHaulContractTest Fill FAIL Active=%d Max=%d"),
				Scenario.ResourceNode->GetActiveMinerCount(),
				Scenario.ResourceNode->GetMaxConcurrentMiners());
			Finish();
			return;
		}
		Expect(Scenario.ResourceNode->GetActiveMinerCount()
				>= Scenario.ResourceNode->GetMaxConcurrentMiners(),
			TEXT("NodeANoFreeMiningSlot"));

		Scenario.Worker->GetCargoComponent()->ClearCargo();
		Scenario.Worker->SetActorLocation(
			NodeALocation + FVector(80.0f, 0.0f, 0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		// B — Issue Mine on full NodeA → SlotFullAlternative reassignment to NodeB.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && IsValid(NodeB) && Cmd != nullptr,
				TEXT("ReassignObjects")))
		{
			Finish();
			return;
		}

		if (MovementWaitTicks == 0)
		{
			IssueMine(Worker, NodeA);
			Expect(Cmd->HasHeldCommand()
					&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
				TEXT("MineAcceptedInitially"));
			Expect(Cmd->GetHeldCommand()->CommandTag != FGPGameplayTags::Get().Command_OrderDrop
					&& Cmd->GetHeldCommand()->CommandTag != FGPGameplayTags::Get().Command_Sell
					&& Cmd->GetHeldCommand()->CommandTag != FGPGameplayTags::Get().Command_Demolish,
				TEXT("BuildingCommandsNotInScope"));
		}

		const bool bOnB = IsAssignedToNode(Worker, NodeB);
		if (WaitCondition(bOnB, TEXT("SlotFullAlternativeTimeout")))
		{
			return;
		}

		Expect(bOnB, TEXT("ReassignedToNodeB"));
		Expect(Cmd->GetMineTarget() != NodeA
				|| (Cmd->HasHeldCommand() && Cmd->GetHeldCommand()->TargetActor.Get() == NodeB),
			TEXT("NotStuckOnFullNodeA"));
		Expect(!HasOwnedLogisticsHub(World, OwnerTag), TEXT("StillNoBuildingInScope"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		// Wait until Worker is actively mining NodeB before forcing cycles.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd != nullptr && Mining != nullptr,
				TEXT("MineBReadyObjects")))
		{
			Finish();
			return;
		}

		const bool bMiningB =
			Mining->IsMining()
			&& Mining->GetCurrentResourceNode() == NodeB
			&& Mining->GetMiningState() == EGP_MiningState::Mining;

		if (!bMiningB && (MovementWaitTicks == 0 || (MovementWaitTicks % 20) == 0))
		{
			Worker->SetActorLocation(
				NodeB->GetActorLocation() + FVector(60.0f, 0.0f, 0.0f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			if (!IsAssignedToNode(Worker, NodeB))
			{
				IssueMine(Worker, NodeB);
			}
		}
		if (WaitCondition(bMiningB, TEXT("BeginMiningNodeBTimeout")))
		{
			return;
		}

		Expect(bMiningB, TEXT("MiningNodeBActive"));
		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		// C/D — Force mining cycles until CargoFull haul starts (no player haul command).
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd != nullptr && Mining != nullptr && Cargo != nullptr,
				TEXT("CargoFullObjects")))
		{
			Finish();
			return;
		}

		if (!Cmd->IsHaulActive()
			&& Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::ReturningToBase)
		{
			for (int32 i = 0; i < 8; ++i)
			{
				if (Cargo->IsFull() || Cmd->IsHaulActive())
				{
					break;
				}
				Mining->DebugForceExecuteMiningCycle();
			}
		}

		const bool bHaulStarted = Cmd->IsHaulActive()
			|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToBase
			|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::DroppingOff;
		if (!Expect(bHaulStarted, TEXT("CargoFullHaulStarted")))
		{
			UE_LOG(LogGPMineReassignmentHaul, Error,
				TEXT("CargoFullHaul miss: Cargo=%.1f Cap=%.1f Full=%s HaulState=%d MineState=%d"),
				Cargo->GetCurrentCargoAmount(),
				Cargo->GetCargoCapacity(),
				Cargo->IsFull() ? TEXT("true") : TEXT("false"),
				static_cast<int32>(Cmd->GetHaulExecutionState()),
				static_cast<int32>(Cmd->GetMineExecutionState()));
			Finish();
			return;
		}

		Expect(Cargo->IsFull() || Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER,
			TEXT("CargoPresentOnHaul"));
		Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("HeldMineKeptDuringHaul_NoPlayerCommand"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.1f);
		break;
	}
	case 4:
	{
		// E — Wait for unload at MainBase (cargo empty, haul finished).
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && Cmd != nullptr && Cargo != nullptr,
				TEXT("UnloadObjects")))
		{
			Finish();
			return;
		}

		const bool bUnloaded = Cargo->IsEmpty()
			&& !Cmd->IsHaulActive()
			&& Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::ReturningToBase
			&& Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::DroppingOff;
		if (WaitCondition(bUnloaded, TEXT("UnloadAtMainBaseTimeout")))
		{
			return;
		}

		Expect(Cargo->IsEmpty(), TEXT("CargoEmptyAfterUnload"));
		Expect(!Cmd->IsHaulActive(), TEXT("HaulInactiveAfterUnload"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 5:
	{
		// F — Assert return to NodeB / continue mining if B viable.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd != nullptr, TEXT("ReturnToBObjects")))
		{
			Finish();
			return;
		}

		const bool bReturningOrMiningB =
			IsAssignedToNode(Worker, NodeB)
			|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching
			|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Mining;

		if (WaitCondition(bReturningOrMiningB, TEXT("ReturnToNodeBTimeout")))
		{
			return;
		}

		Expect(bReturningOrMiningB, TEXT("ReturnToNodeBContinueMining"));
		Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("HeldMineAfterUnload"));
		Expect(IsValid(NodeB) && !NodeB->IsDepleted() && NodeB->GetCurrentAmount() > 0,
			TEXT("NodeBStillViable"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		// G — After unload empty: AddCargo to full, IssueMine → MineRejected CargoFull.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && IsValid(NodeB) && Cmd != nullptr && Cargo != nullptr,
				TEXT("RejectObjects")))
		{
			Finish();
			return;
		}

		AActor* HeldBefore = Cmd->HasHeldCommand() ? Cmd->GetHeldCommand()->TargetActor.Get() : nullptr;

		const float Cap = Cargo->GetCargoCapacity();
		Cargo->AddCargo(Cap);
		Expect(Cargo->IsFull(), TEXT("CargoFilledManually"));

		IssueMine(Worker, NodeA);

		Expect(Cargo->IsFull(), TEXT("CargoStillFullAfterRejectedMine"));
		Expect(Cmd->GetMineTarget() != NodeA, TEXT("MineNotAcceptedOnNodeAWhileFull"));
		const bool bHeldSwitchedToNodeA = Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine
			&& Cmd->GetHeldCommand()->TargetActor.Get() == NodeA
			&& HeldBefore != NodeA;
		Expect(!bHeldSwitchedToNodeA, TEXT("HeldDidNotSwitchToNodeA"));
		Expect(!(Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching
					&& Cmd->GetMineTarget() == NodeA)
				&& !(Cmd->GetMineExecutionState() == EGP_MineExecutionState::Active
					&& Cmd->GetMineTarget() == NodeA),
			TEXT("MineExecutionDidNotRestartOnNodeA"));
		Expect(!Cmd->IsHaulActive(), TEXT("RejectDidNotStartHaulFromRejectedMineA"));
		Expect(!HasOwnedLogisticsHub(World, OwnerTag), TEXT("BuildingStillOutOfScope"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		Expect(!Worker || !Worker->PrimaryActorTick.bCanEverTick, TEXT("WorkerNoPermanentTick"));
		Expect(!HasOwnedLogisticsHub(World, OwnerTag), TEXT("FinalNoBuildingInScope"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_MineReassignmentHaulContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_MineReassignmentHaulContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_MineReassignmentHaulContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_MineReassignmentHaulContractTestRunner::AdvanceStage() {}
bool UGP_MineReassignmentHaulContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_MineReassignmentHaulContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_MineReassignmentHaulContractTestRunner::Finish() { bFinished = true; }
void UGP_MineReassignmentHaulContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_MineReassignmentHaulContractTestRunner::UnbindWorldCleanup() {}
void UGP_MineReassignmentHaulContractTestRunner::CleanupActors() {}

#endif
