// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPMineReassignmentHaulContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NavAreas/NavArea_Null.h"
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

	static bool OwnershipOnNodeBProven(AGP_Worker* Worker, AGP_ResourceNode* NodeB, uint32 ExpectedSerial)
	{
		if (!IsValid(Worker) || !IsValid(NodeB))
		{
			return false;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		UGP_MiningComponent* Mining = Worker->GetMiningComponent();
		if (Cmd == nullptr || Mining == nullptr)
		{
			return false;
		}
		return Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine
			&& Cmd->GetHeldCommand()->TargetActor.Get() == NodeB
			&& Cmd->GetMineTarget() == NodeB
			&& Cmd->GetActiveMineSerial() != 0
			&& (ExpectedSerial == 0 || Cmd->GetActiveMineSerial() == ExpectedSerial)
			&& Mining->GetCurrentResourceNode() == NodeB
			&& Mining->GetMiningState() == EGP_MiningState::Mining
			&& Cmd->DebugIsMiningStateEventBound();
	}

	static bool HaulActiveTowardBase(UGP_UnitCommandComponent* Cmd)
	{
		if (Cmd == nullptr)
		{
			return false;
		}
		return Cmd->IsHaulActive()
			|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToBase
			|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::DroppingOff;
	}

	/** Operator-like MainBase NavArea_Null footprint (~ClearanceHalfXY 218.2). */
	static void ActivateMainBaseNavigationObstacle(AGP_MainBase* Base)
	{
		if (!IsValid(Base))
		{
			return;
		}
		UBoxComponent* NavBox = Base->GetNavigationObstacle();
		if (NavBox == nullptr)
		{
			return;
		}
		NavBox->SetBoxExtent(FVector(218.2f, 218.2f, 130.0f));
		NavBox->SetCanEverAffectNavigation(true);
		NavBox->bDynamicObstacle = true;
		NavBox->SetAreaClassOverride(UNavArea_Null::StaticClass());
		NavBox->UpdateBounds();
		NavBox->MarkRenderStateDirty();
	}

	/** Elevate MainBase actor origin so legacy 3D approach geometry fails (operator DeltaZ repro). */
	static void ElevateMainBaseOriginForDeltaZRepro(AGP_MainBase* Base, float DeltaZCm = 280.0f)
	{
		if (!IsValid(Base))
		{
			return;
		}
		const FVector Loc = Base->GetActorLocation();
		Base->SetActorLocation(
			FVector(Loc.X, Loc.Y, Loc.Z + DeltaZCm),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	/** Force candidate index 0 (direct radial) unavailable so haul must pick an alternate sector. */
	static constexpr int32 SkipDirectRadialCandidateMask = 1 << 0;

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
		TEXT("Authority: GP-S33M natural Mine(A) SlotFull→B → CargoFull haul → unload → return. No teleport/repair IssueMine."),
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
	DestroyWeak(Worker2Weak);
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
	InitialMineSerial = 0;
	Worker2MineSerial = 0;
	bOwnershipProven = false;
	bCargoFullObserved = false;
	bIssueMineAfterInitial = false;
	bTeleportedAfterInitial = false;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_MineReassignmentHaulContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMineReassignmentHaul, Log,
		TEXT("gp.Resource.RunMineReassignmentHaulContractTest Start (natural chain — no teleport/repair Mine)"));
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

		ActivateMainBaseNavigationObstacle(Scenario.MainBase);
		ElevateMainBaseOriginForDeltaZRepro(Scenario.MainBase, 280.0f);
		MainBaseLocation = Scenario.MainBase->GetActorLocation();
		Expect(Scenario.MainBase->GetNavigationObstacle() != nullptr
				&& Scenario.MainBase->GetNavigationObstacle()->CanEverAffectNavigation(),
			TEXT("MainBaseNavigationObstacleActive"));
		Expect(FMath::Abs(MainBaseLocation.Z - Scenario.Worker->GetActorLocation().Z) > 200.0f,
			TEXT("MainBaseElevatedDeltaZ_ReproOld3DFailure"));

		FString NodeBFail;
		AGP_ResourceNode* NodeB = SpawnNodeBNearA(
			World, OwnerTag, NodeALocation, TestSearchRadiusCm, NodeBFail);
		if (!Expect(IsValid(NodeB), TEXT("SpawnNodeBNearA")))
		{
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
			Finish();
			return;
		}

		// Place tested workers near A (before Mine only — no teleport after IssueMine).
		Scenario.Worker->GetCargoComponent()->ClearCargo();
		Scenario.Worker->SetActorLocation(
			NodeALocation + FVector(120.0f, 40.0f, 0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (UGP_UnitCommandComponent* Cmd1Setup = Scenario.Worker->GetUnitCommandComponent())
		{
			// Radial haul candidate forced unavailable — must choose alternate around MainBase.
			Cmd1Setup->DebugSetApproachSkipCandidateMask(SkipDirectRadialCandidateMask);
		}

		AGP_Worker* Worker2 = GPResourceLoopDiagnostics::SpawnWorkerDeferred(
			World, NodeALocation + FVector(120.0f, -40.0f, 0.0f), ContractTeamId, OwnerTag);
		if (!Expect(IsValid(Worker2), TEXT("SpawnSecondTestedWorker")))
		{
			Finish();
			return;
		}
		Worker2->GetCargoComponent()->ClearCargo();
		if (UGP_UnitCommandComponent* Cmd2Setup = Worker2->GetUnitCommandComponent())
		{
			Cmd2Setup->DebugSetApproachSkipCandidateMask(SkipDirectRadialCandidateMask);
		}
		Worker2Weak = Worker2;

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		// ONE Mine(A) per tested worker — then never IssueMine / teleport again.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_Worker* Worker2 = Worker2Weak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_UnitCommandComponent* Cmd2 = IsValid(Worker2) ? Worker2->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Worker2) && IsValid(NodeA) && Cmd && Cmd2,
				TEXT("InitialMineObjects")))
		{
			Finish();
			return;
		}

		IssueMine(Worker, NodeA);
		IssueMine(Worker2, NodeA);

		Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("MineAcceptedInitially_Worker1"));
		Expect(Cmd2->HasHeldCommand()
				&& Cmd2->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("MineAcceptedInitially_Worker2"));

		InitialMineSerial = Cmd->GetActiveMineSerial();
		Worker2MineSerial = Cmd2->GetActiveMineSerial();
		Expect(InitialMineSerial != 0, TEXT("Worker1MineSerialAllocated"));
		Expect(Worker2MineSerial != 0, TEXT("Worker2MineSerialAllocated"));
		// Serials are per-UnitCommandComponent (both may be 1). Independence = separate component state.
		Expect(Cmd != Cmd2, TEXT("WorkersHaveIndependentCommandComponents"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd != nullptr, TEXT("ReassignObjects")))
		{
			Finish();
			return;
		}

		const bool bHeldB = Cmd->HasHeldCommand()
			&& Cmd->GetHeldCommand()->TargetActor.Get() == NodeB;
		const bool bMineTargetB = Cmd->GetMineTarget() == NodeB;
		if (WaitCondition(bHeldB && bMineTargetB, TEXT("ReassignedHeldTargetIsB_Timeout")))
		{
			return;
		}

		Expect(bHeldB, TEXT("ReassignedHeldTargetIsB"));
		Expect(bMineTargetB, TEXT("ReassignedMineTargetIsB"));
		Expect(Cmd->GetActiveMineSerial() == InitialMineSerial, TEXT("SameMineSerialPreserved"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		// Natural move + BeginMining on B — no teleport, no IssueMine.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd && Mining, TEXT("MineBNaturalObjects")))
		{
			Finish();
			return;
		}

		const bool bProven = OwnershipOnNodeBProven(Worker, NodeB, InitialMineSerial);
		if (WaitCondition(bProven, TEXT("MiningBActive_NaturalTimeout")))
		{
			return;
		}

		Expect(Mining->GetCurrentResourceNode() == NodeB, TEXT("MiningComponentTargetIsB"));
		Expect(bProven, TEXT("MiningBActive"));
		Expect(Cmd->DebugIsMiningStateEventBound(), TEXT("MiningStateBindingAlive"));
		Expect(Cmd->GetActiveMineSerial() == InitialMineSerial, TEXT("SameMineSerialPreserved_AtMining"));
		bOwnershipProven = true;

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		// Accelerate ONLY after ownership proven — never repairs state.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd && Mining && Cargo, TEXT("CargoFullObjects")))
		{
			Finish();
			return;
		}
		if (!Expect(bOwnershipProven && OwnershipOnNodeBProven(Worker, NodeB, InitialMineSerial),
				TEXT("OwnershipStillValidBeforeForceCycles")))
		{
			Finish();
			return;
		}

		if (!HaulActiveTowardBase(Cmd) && !Cargo->IsFull())
		{
			for (int32 i = 0; i < 12; ++i)
			{
				if (!OwnershipOnNodeBProven(Worker, NodeB, InitialMineSerial) && !Cargo->IsFull())
				{
					Expect(false, TEXT("OwnershipLostDuringForceCycles"));
					Finish();
					return;
				}
				if (Cargo->IsFull() || HaulActiveTowardBase(Cmd))
				{
					break;
				}
				Mining->DebugForceExecuteMiningCycle();
			}
		}

		if (Cargo->IsFull()
			|| Mining->GetMiningState() == EGP_MiningState::CargoFull
			|| HaulActiveTowardBase(Cmd))
		{
			bCargoFullObserved = true;
		}

		if (WaitCondition(HaulActiveTowardBase(Cmd), TEXT("HaulStartedAutomatically_Timeout")))
		{
			return;
		}

		Expect(bCargoFullObserved || Cargo->IsFull() || HaulActiveTowardBase(Cmd),
			TEXT("CargoFullEventObserved"));
		Expect(HaulActiveTowardBase(Cmd), TEXT("HaulStartedAutomatically"));
		Expect(Cmd->GetLastHaulDeposit() == NodeB, TEXT("HaulDepositIsB"));
		Expect(Cmd->GetActiveHaulSerial() == InitialMineSerial
				|| Cmd->GetActiveMineSerial() == InitialMineSerial,
			TEXT("HaulSerialMatchesMineSerial"));
		Expect(Cmd->HasHeldCommand()
				&& Cmd->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("HeldMineKeptDuringHaul_NoPlayerCommand"));
		Expect(Cmd->DebugGetLastApproachCandidateIndex() > 0,
			TEXT("HaulChoseAlternateCandidate_NotRadial0"));
		Expect(!bIssueMineAfterInitial, TEXT("Guard_NoIssueMineAfterInitial"));
		Expect(!bTeleportedAfterInitial, TEXT("Guard_NoTeleportAfterInitial"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.1f);
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Base) && Cmd && Cargo, TEXT("UnloadObjects")))
		{
			Finish();
			return;
		}

		const bool bUnloaded = Cargo->IsEmpty()
			&& !Cmd->IsHaulActive()
			&& Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::ReturningToBase
			&& Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::DroppingOff;
		if (WaitCondition(bUnloaded, TEXT("ReachedMainBase_UnloadTimeout")))
		{
			return;
		}

		Expect(bUnloaded, TEXT("ReachedMainBase"));
		Expect(Cargo->IsEmpty(), TEXT("CargoEmptyAfterUnload"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd && Mining, TEXT("ReturnToBObjects")))
		{
			Finish();
			return;
		}

		const bool bReturnOrMine =
			Cmd->GetMineTarget() == NodeB
			|| (Cmd->HasHeldCommand() && Cmd->GetHeldCommand()->TargetActor.Get() == NodeB
				&& (Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching
					|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::Active
					|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit
					|| Mining->GetCurrentResourceNode() == NodeB
					|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine
					|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Mining));

		if (WaitCondition(bReturnOrMine, TEXT("ReturnToBOrResumeMining_Timeout")))
		{
			return;
		}

		Expect(bReturnOrMine, TEXT("ReturnToBOrResumeMining"));
		Expect(IsValid(NodeB) && !NodeB->IsDepleted(), TEXT("NodeBStillViable"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 7:
	{
		// Concurrent worker2: independent serial; natural reassignment; CargoFull→haul when ownership proven.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_Worker* Worker2 = Worker2Weak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd1 = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_UnitCommandComponent* Cmd2 = IsValid(Worker2) ? Worker2->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining2 = IsValid(Worker2) ? Worker2->GetMiningComponent() : nullptr;
		UGP_CargoComponent* Cargo2 = IsValid(Worker2) ? Worker2->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker2) && IsValid(NodeB) && Cmd1 && Cmd2 && Mining2 && Cargo2,
				TEXT("Worker2Objects")))
		{
			Finish();
			return;
		}

		// Worker2 must keep its own chain identity on its own component (not cleared by Worker1 haul).
		Expect(Cmd2->HasHeldCommand()
				&& Cmd2->GetHeldCommand()->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("Worker2HeldMineIndependent"));
		Expect(Cmd2->GetActiveMineSerial() == Worker2MineSerial
				|| Cmd2->GetActiveHaulSerial() == Worker2MineSerial
				|| HaulActiveTowardBase(Cmd2)
				|| OwnershipOnNodeBProven(Worker2, NodeB, Worker2MineSerial),
			TEXT("Worker2OwnSerialPreserved"));

		const bool bW2Ready =
			OwnershipOnNodeBProven(Worker2, NodeB, Worker2MineSerial)
			|| HaulActiveTowardBase(Cmd2)
			|| (Cmd2->HasHeldCommand() && Cmd2->GetHeldCommand()->TargetActor.Get() == NodeB
				&& Cmd2->GetActiveMineSerial() == Worker2MineSerial);

		if (WaitCondition(bW2Ready, TEXT("Worker2ReassignedOrHauling_Timeout")))
		{
			return;
		}

		if (OwnershipOnNodeBProven(Worker2, NodeB, Worker2MineSerial) && !HaulActiveTowardBase(Cmd2))
		{
			for (int32 i = 0; i < 12 && !HaulActiveTowardBase(Cmd2) && !Cargo2->IsFull(); ++i)
			{
				Mining2->DebugForceExecuteMiningCycle();
			}
		}

		if (WaitCondition(
				HaulActiveTowardBase(Cmd2)
					|| Cargo2->IsEmpty()
					|| OwnershipOnNodeBProven(Worker2, NodeB, Worker2MineSerial),
				TEXT("Worker2HaulOrContinue_Timeout")))
		{
			return;
		}

		Expect(HaulActiveTowardBase(Cmd2)
				|| Cargo2->IsEmpty()
				|| OwnershipOnNodeBProven(Worker2, NodeB, Worker2MineSerial),
			TEXT("Worker2IndependentChainProgress"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		// Explicit Mine while CargoFull is accepted and starts a mining-cycle haul back to NodeA.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && Cmd && Cargo, TEXT("FullCargoMineObjects")))
		{
			Finish();
			return;
		}

		const bool bIdleEnough = !HaulActiveTowardBase(Cmd);
		if (WaitCondition(bIdleEnough, TEXT("WaitHaulIdleBeforeFullCargoMine")))
		{
			return;
		}

		Cargo->ClearCargo();
		Cargo->AddCargo(Cargo->GetCargoCapacity());
		Expect(Cargo->IsFull(), TEXT("CargoFilledManuallyForFullCargoMine"));

		IssueMine(Worker, NodeA);

		const FGP_StoredUnitCommand* Held = Cmd->GetHeldCommand();
		Expect(Cmd->HasHeldCommand() && Held != nullptr
				&& Held->CommandTag == FGPGameplayTags::Get().Command_Mine,
			TEXT("FullCargoMineAcceptedHeld"));
		Expect(Held != nullptr && Held->TargetActor.Get() == NodeA, TEXT("FullCargoMineAssignmentIsNodeA"));
		Expect(Cmd->ShouldReturnToDepositAfterHaul(), TEXT("FullCargoMineReturnToDepositArmed"));
		Expect(HaulActiveTowardBase(Cmd)
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::DroppingOff,
			TEXT("FullCargoMineStartsHaul"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9:
	{
		Expect(!bTeleportedAfterInitial, TEXT("NaturalChain_NoTeleportUsed"));
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
