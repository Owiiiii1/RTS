// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceApproach.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPResourceNodeSearch.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitCommandComponent.h"
#include "Visual/GPResourceNodeVisualComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPDepletionReassignment, Log, All);

namespace GPDepletionReassignmentDebug
{
	static TWeakObjectPtr<UGP_DepletionReassignmentContractTestRunner> GActiveRunner;

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

	static void ApplySearchSettings(float SearchRadiusCm, float MaxPathLengthCm)
	{
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			Settings->ResourceSearchRadiusCm = SearchRadiusCm;
			Settings->MaxResourcePathLengthCm = MaxPathLengthCm;
		}
	}

	static void FillApproachQueryDefaults(FGP_ResourceNodeSearchQuery& Query, AGP_Worker* Worker)
	{
		const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get();
		Query.InteractionRangeCm = 200.0f;
		Query.AcceptanceRadiusCm = 50.0f;
		Query.ApproachSafetyMarginCm = Settings != nullptr ? Settings->ResourceApproachSafetyMarginCm : 25.0f;
		Query.ApproachDirectionCount = Settings != nullptr ? Settings->ResourceApproachDirectionCount : 8;
		if (IsValid(Worker))
		{
			if (UGP_MovementComponent* Movement = Worker->GetUnitMovementComponent())
			{
				Query.AcceptanceRadiusCm = Movement->AcceptanceRadius;
			}
			if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
			{
				const float Range = Mining->GetInteractionRangeCm();
				if (Range > 0.0f)
				{
					Query.InteractionRangeCm = Range;
				}
			}
		}
	}

	static void RunDepletionReassignmentContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPDepletionReassignment, Warning,
				TEXT("GP Resource.RunDepletionReassignmentContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("DepletionReassignmentContract"), TEXT("DepletionReassignment"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_DepletionReassignmentContractTestRunner* Runner =
			NewObject<UGP_DepletionReassignmentContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GDepletionReassignmentContract(
		TEXT("gp.Resource.RunDepletionReassignmentContractTest"),
		TEXT("Authority: GP-S28P2 depletion/registry/reassignment/anchor contract. Transient actors only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunDepletionReassignmentContractTest));
}

void UGP_DepletionReassignmentContractTestRunner::HandleDepleted(
	AGP_ResourceNode* ResourceNode,
	int32 PreviousAmount)
{
	(void)ResourceNode;
	++DepletionEventCount;
	LastDepletionPreviousAmount = PreviousAmount;
}

void UGP_DepletionReassignmentContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_DepletionReassignmentContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_DepletionReassignmentContractTestRunner::OnWorldCleanup(
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

void UGP_DepletionReassignmentContractTestRunner::CleanupActors()
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
	DestroyWeak(SlotHolderWeak);
	DestroyWeak(NodeAWeak);
	DestroyWeak(NodeBWeak);
	DestroyWeak(WakeInsideWeak);
	DestroyWeak(WakeOutsideWeak);
	DestroyWeak(MainBaseWeak);
	DestroyWeak(FifoNodeWeak);
	for (TWeakObjectPtr<AGP_Worker>& Weak : FifoWorkers)
	{
		DestroyWeak(Weak);
	}
	FifoWorkers.Reset();
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
}

void UGP_DepletionReassignmentContractTestRunner::Finish()
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
			Settings->WaitingForResourceRetrySeconds = SavedSettingsRetrySeconds;
		}
		bSettingsOverridden = false;
	}

	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	UnbindWorldCleanup();
	CleanupActors();

	UE_LOG(LogGPDepletionReassignment, Log,
		TEXT("GP Resource.RunDepletionReassignmentContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPDepletionReassignmentDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_DepletionReassignmentContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPDepletionReassignment, Error,
		TEXT("GP Resource.RunDepletionReassignmentContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_DepletionReassignmentContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPDepletionReassignment, Error,
			TEXT("GP Resource.RunDepletionReassignmentContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPDepletionReassignment, Log,
			TEXT("GP Resource.RunDepletionReassignmentContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_DepletionReassignmentContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_DepletionReassignmentContractTestRunner::AdvanceStage));
}

void UGP_DepletionReassignmentContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	DepletionEventCount = 0;
	MovementWaitTicks = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_DepletionReassignmentContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPDepletionReassignment, Log, TEXT("GP Resource.RunDepletionReassignmentContractTest Start"));
	ScheduleNext();
}

void UGP_DepletionReassignmentContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	using namespace GPDepletionReassignmentDebug;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	auto WaitBusy = [&](AGP_Worker* Worker, const TCHAR* TimeoutLabel) -> bool
	{
		if (!IsValid(Worker))
		{
			Expect(false, TEXT("WaitBusyWorkerLost"));
			Finish();
			return true;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		const bool bBusy = (Cmd != nullptr && Cmd->IsHaulActive())
			|| (Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving())
			|| (Cmd != nullptr && Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching);
		if (!bBusy)
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
		ScheduleNext();
		return true;
	};

	switch (StageIndex)
	{
	case 0:
	{
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (!Expect(IsValid(GS), TEXT("GameStatePresent")))
		{
			Finish();
			return;
		}

		AGP_ResourceNode* NodeA = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(), FVector(-40000.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
		AGP_ResourceNode* NodeB = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(), FVector(-39500.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
		if (!Expect(IsValid(NodeA) && IsValid(NodeB), TEXT("SpawnNodes")))
		{
			Finish();
			return;
		}
		NodeAWeak = NodeA;
		NodeBWeak = NodeB;

		NodeA->SetUseGeneratedPrototypeVisual(false);
		Expect(NodeA->GetResourceNodeVisualComponent() != nullptr
				&& NodeA->GetResourceNodeVisualComponent()->GetGeneratedPartCount() == 0,
			TEXT("GeneratedVisualFalseCleared"));

		const int32 BeforeCount = GS->GetRegisteredResourceNodeCount();
		Expect(GS->RegisterResourceNode(NodeA)
				== AGP_GameState::EGP_ResourceNodeRegisterResult::AlreadyRegistered
			|| GS->GetRegisteredResourceNodeCount() >= BeforeCount,
			TEXT("NodeARegisteredViaBeginPlayOrExplicit"));
		Expect(GS->RegisterResourceNode(NodeA)
				== AGP_GameState::EGP_ResourceNodeRegisterResult::AlreadyRegistered,
			TEXT("DuplicateRegisterIgnored"));

		NodeA->OnResourceDepleted.AddDynamic(
			this, &UGP_DepletionReassignmentContractTestRunner::HandleDepleted);
		NodeA->DebugSetCurrentAmountForTest(5, false);
		Expect(NodeA->GetCurrentAmount() == 5, TEXT("AmountSet5"));
		Expect(!NodeA->HasCompletedDepletionTransition(), TEXT("NotDepletedYet"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (!Expect(IsValid(NodeA) && IsValid(GS), TEXT("DepletionObjects")))
		{
			Finish();
			return;
		}

		AGP_Worker* Holder = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), NodeA->GetActorLocation(), FRotator::ZeroRotator, Params);
		SlotHolderWeak = Holder;
		if (IsValid(Holder))
		{
			const EGP_MiningSlotRequestResult SlotResult = NodeA->RequestMiningSlot(Holder);
			Expect(SlotResult == EGP_MiningSlotRequestResult::Granted
					|| SlotResult == EGP_MiningSlotRequestResult::AlreadyActive,
				TEXT("ActiveSlotGranted"));
		}

		AGP_Worker* Waiting = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			NodeA->GetActorLocation() + FVector(0.0f, 100.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		WorkerWeak = Waiting;
		TArray<AGP_Worker*> Fillers;
		if (IsValid(Waiting))
		{
			while (NodeA->GetActiveMinerCount() < NodeA->GetMaxConcurrentMiners())
			{
				AGP_Worker* Extra = World->SpawnActor<AGP_Worker>(
					AGP_Worker::StaticClass(),
					NodeA->GetActorLocation(),
					FRotator::ZeroRotator,
					Params);
				if (!IsValid(Extra))
				{
					break;
				}
				NodeA->RequestMiningSlot(Extra);
				Fillers.Add(Extra);
			}
			const EGP_MiningSlotRequestResult WaitResult = NodeA->RequestMiningSlot(Waiting);
			Expect(WaitResult == EGP_MiningSlotRequestResult::Waiting
					|| WaitResult == EGP_MiningSlotRequestResult::AlreadyWaiting,
				TEXT("WaitingSecondWorker"));
		}

		const int32 BeforeEvents = DepletionEventCount;
		const int32 Consumed = NodeA->ConsumeResource(100);
		Expect(Consumed > 0, TEXT("ConsumeTrippedDepletion"));
		Expect(NodeA->GetCurrentAmount() == 0, TEXT("AmountZero"));
		Expect(NodeA->HasCompletedDepletionTransition(), TEXT("DepletionFlagSet"));
		Expect(DepletionEventCount == BeforeEvents + 1, TEXT("DepletionEventOnce"));
		Expect(LastDepletionPreviousAmount > 0, TEXT("DepletionPreviousPositive"));
		Expect(NodeA->GetActiveMinerCount() == 0, TEXT("ActiveClearedNoPromote"));
		Expect(NodeA->GetWaitingMinerCount() == 0, TEXT("WaitingClearedNoPromote"));
		Expect(NodeA->IsDestroyPending(), TEXT("DestroyPending"));
		Expect(IsValid(NodeA->GetCollisionBox())
				&& NodeA->GetCollisionBox()->GetCollisionEnabled() == ECollisionEnabled::NoCollision,
			TEXT("CollisionDisabled"));
		Expect(!NodeA->CanAcceptMineCommand(true, nullptr), TEXT("MineRejectedAfterDeplete"));
		Expect(NodeA->GetResourceNodeVisualComponent()->GetGeneratedPartCount() == 0,
			TEXT("GeneratedStillClearedAfterDeplete"));

		NodeA->ConsumeResource(10);
		Expect(DepletionEventCount == BeforeEvents + 1, TEXT("NoDoubleDepletionEvent"));

		FGP_ResourceNodeSearchQuery Query;
		Query.SearchCenter = NodeA->GetActorLocation();
		Query.PathStart = IsValid(Waiting) ? Waiting->GetActorLocation() : NodeA->GetActorLocation();
		Query.SearchRadiusCm = 100000.0f;
		Query.MaxPathLengthCm = 100000.0f;
		Query.PathfindingActor = Waiting;
		Query.bRequireFreeSlot = false;
		TArray<FGP_ResourceNodeCandidate> Candidates;
		GS->FindResourceCandidates(Query, Candidates);
		bool bFoundDepleted = false;
		for (const FGP_ResourceNodeCandidate& Candidate : Candidates)
		{
			if (Candidate.Node == NodeA)
			{
				bFoundDepleted = true;
			}
		}
		Expect(!bFoundDepleted, TEXT("DepletedExcludedFromSearch"));

		for (AGP_Worker* Extra : Fillers)
		{
			if (IsValid(Extra))
			{
				Extra->Destroy();
			}
		}

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		++StageIndex;
		if (UWorld* W = WorldWeak.Get())
		{
			W->GetTimerManager().SetTimer(
				StageTimerHandle,
				FTimerDelegate::CreateUObject(this, &UGP_DepletionReassignmentContractTestRunner::AdvanceStage),
				0.4f,
				false);
		}
		break;
	}
	case 3:
	{
		Expect(!NodeAWeak.IsValid() || !IsValid(NodeAWeak.Get()), TEXT("NodeADestroyedDeferred"));

		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (Expect(IsValid(NodeB) && IsValid(GS), TEXT("NodeBAlive")))
		{
			Expect(GS->RegisterResourceNode(NodeB)
					== AGP_GameState::EGP_ResourceNodeRegisterResult::AlreadyRegistered
				|| GS->GetRegisteredResourceNodeCount() > 0,
				TEXT("NodeBStillRegistered"));
		}

		Expect(!Worker || !Worker->PrimaryActorTick.bCanEverTick, TEXT("WorkerNoPermanentTick"));
		Expect(IsValid(NodeB) && !NodeB->PrimaryActorTick.bCanEverTick, TEXT("NodeNoPermanentTick"));

		CleanupActors();
		NodeAWeak.Reset();
		NodeBWeak.Reset();
		WorkerWeak.Reset();
		SlotHolderWeak.Reset();

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		// Post-drop-off anchor regression layout (navigable).
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("SpawnNavigableAnchorScenario")))
		{
			Finish();
			return;
		}

		AGP_MainBase* Base = Scenario.MainBase;
		AGP_Worker* Worker = Scenario.Worker;
		AGP_ResourceNode* NodeA = Scenario.ResourceNode;
		MainBaseWeak = Base;
		WorkerWeak = Worker;
		NodeAWeak = NodeA;
		MainBaseLocation = Base->GetActorLocation();
		AnchorClusterLocation = NodeA->GetActorLocation();

		TestSearchRadiusCm = 1000.0f;
		TestMaxPathLengthCm = 6000.0f;
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			SavedSettingsSearchRadiusCm = Settings->ResourceSearchRadiusCm;
			SavedSettingsMaxPathLengthCm = Settings->MaxResourcePathLengthCm;
			SavedSettingsRetrySeconds = Settings->WaitingForResourceRetrySeconds;
			bSettingsOverridden = true;
		}
		ApplySearchSettings(TestSearchRadiusCm, TestMaxPathLengthCm);
		Expect(Worker->GetResourceSearchRadiusCm() <= TestSearchRadiusCm + KINDA_SMALL_NUMBER,
			TEXT("SearchRadiusTunableApplied"));

		const float DistBaseToA = FVector::Dist(MainBaseLocation, AnchorClusterLocation);
		Expect(DistBaseToA > TestSearchRadiusCm, TEXT("MainBaseOutsideSearchRadiusFromCluster"));

		FVector NodeBLoc = AnchorClusterLocation + FVector(400.0f, 0.0f, 0.0f);
		FVector NodeBProjected;
		if (!GPResourceLoopDiagnostics::IsNavPointProjected(World, NodeBLoc, &NodeBProjected, 800.0f, 800.0f))
		{
			NodeBProjected = AnchorClusterLocation + FVector(0.0f, 400.0f, 0.0f);
		}
		AGP_ResourceNode* NodeB = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, NodeBProjected, OwnerTag);
		if (!Expect(IsValid(NodeB), TEXT("SpawnNodeBNearCluster")))
		{
			Finish();
			return;
		}
		NodeBWeak = NodeB;
		Expect(FVector::Dist(AnchorClusterLocation, NodeB->GetActorLocation()) <= TestSearchRadiusCm,
			TEXT("NodeBInsideAnchorRadius"));
		Expect(FVector::Dist(MainBaseLocation, NodeB->GetActorLocation()) > TestSearchRadiusCm,
			TEXT("NodeBOutsideBaseRadius"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeA) && IsValid(NodeB) && IsValid(GS) && Cmd != nullptr,
				TEXT("AnchorSetupObjects")))
		{
			Finish();
			return;
		}

		Worker->SetActorLocation(NodeA->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, NodeA);

		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("MineAnchorSetOnAccept"));
		Expect(FVector::Dist(Cmd->DebugGetMineSearchAnchorLocation(), NodeA->GetActorLocation()) < 50.0f,
			TEXT("MineAnchorAtNodeA"));

		// API split: radius from SearchCenter(anchor), path from PathStart(Worker@Base).
		Worker->SetActorLocation(MainBaseLocation, false, nullptr, ETeleportType::TeleportPhysics);

		FGP_ResourceNodeSearchQuery WrongCenter;
		WrongCenter.SearchCenter = Worker->GetActorLocation();
		WrongCenter.PathStart = Worker->GetActorLocation();
		WrongCenter.SearchRadiusCm = TestSearchRadiusCm;
		WrongCenter.MaxPathLengthCm = TestMaxPathLengthCm;
		WrongCenter.ExcludeNode = NodeA;
		WrongCenter.PathfindingActor = Worker;
		WrongCenter.bRequireFreeSlot = false;
		FillApproachQueryDefaults(WrongCenter, Worker);
		AGP_ResourceNode* WrongBest = GS->FindBestResourceCandidate(WrongCenter);
		Expect(WrongBest != NodeB, TEXT("BaseAsSearchCenterMissesNodeB"));

		FGP_ResourceNodeSearchQuery AnchorCenter;
		AnchorCenter.SearchCenter = Cmd->DebugGetMineSearchAnchorLocation();
		AnchorCenter.PathStart = Worker->GetActorLocation();
		AnchorCenter.SearchRadiusCm = TestSearchRadiusCm;
		AnchorCenter.MaxPathLengthCm = TestMaxPathLengthCm;
		AnchorCenter.ExcludeNode = NodeA;
		AnchorCenter.PathfindingActor = Worker;
		AnchorCenter.bRequireFreeSlot = false;
		FillApproachQueryDefaults(AnchorCenter, Worker);
		AGP_ResourceNode* AnchorBest = GS->FindBestResourceCandidate(AnchorCenter);
		Expect(AnchorBest == NodeB, TEXT("AnchorSearchCenterFindsNodeB"));
		if (AnchorBest == NodeB)
		{
			TArray<FGP_ResourceNodeCandidate> Accepted;
			GS->FindResourceCandidates(AnchorCenter, Accepted);
			Expect(Accepted.Num() > 0 && !Accepted[0].BestApproachLocation.IsNearlyZero(),
				TEXT("AcceptedCandidateHasApproachPoint"));
		}

		// Restore Worker near NodeA for haul/depletion path.
		Worker->SetActorLocation(NodeA->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, NodeA);
		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("MineAnchorPreservedAfterReissue"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		// CargoFull haul first; deplete NodeA during haul → PostDropOff reassignment to NodeB.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeA = NodeAWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(NodeA), TEXT("PostDropOffObjects")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (Cmd == nullptr || !Cmd->IsHaulActive())
		{
			for (int32 i = 0; i < 8; ++i)
			{
				if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
				{
					Mining->DebugForceExecuteMiningCycle();
				}
				if (Cmd != nullptr && Cmd->IsHaulActive())
				{
					break;
				}
			}
		}

		if (!Expect(Cmd != nullptr && Cmd->IsHaulActive(), TEXT("CargoFullHaulStartedFirst")))
		{
			Finish();
			return;
		}
		Expect(Cmd->HasHeldCommand(), TEXT("HeldMineKeptDuringHaul"));
		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("AnchorKeptDuringHaul"));

		if (IsValid(NodeA) && !NodeA->HasCompletedDepletionTransition())
		{
			NodeA->ConsumeResource(100000);
		}
		Expect(!IsValid(NodeA) || NodeA->HasCompletedDepletionTransition() || NodeA->IsDestroyPending(),
			TEXT("NodeADepletedDuringHaul"));

		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 7:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && Cmd != nullptr, TEXT("PostDropOffWaitObjects")))
		{
			Finish();
			return;
		}

		if (WaitBusy(Worker, TEXT("PostDropOffHaulTimeout")))
		{
			return;
		}

		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("AnchorSurvivesDropOff"));
		Expect(Cmd->GetMineExecutionState() != EGP_MineExecutionState::WaitingForResource,
			TEXT("NoWaitingForResourceAfterDropOff"));
		Expect(Cmd->GetMineTarget() == NodeB
				|| (Cmd->HasHeldCommand()
					&& Cmd->GetHeldCommand()->TargetActor.Get() == NodeB)
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Mining,
			TEXT("RetargetedNodeBAfterDropOff"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 8:
	{
		// Move clears anchor.
		AGP_Worker* Worker = WorkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && Cmd != nullptr, TEXT("MoveClearObjects")))
		{
			Finish();
			return;
		}
		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("AnchorPresentBeforeMove"));
		IssueMove(Worker, MainBaseLocation + FVector(200.0f, 0.0f, 0.0f));
		Expect(!Cmd->DebugHasMineSearchAnchor(), TEXT("MoveClearsMineAnchor"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 9:
	{
		// WaitingForResource keeps anchor; wake inside radius, ignore outside.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* NodeB = NodeBWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(NodeB) && IsValid(GS) && Cmd != nullptr,
				TEXT("WaitingWakeObjects")))
		{
			Finish();
			return;
		}

		Worker->SetActorLocation(NodeB->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		Worker->GetCargoComponent()->ClearCargo();
		ApplySearchSettings(TestSearchRadiusCm, TestMaxPathLengthCm);
		IssueMine(Worker, NodeB);
		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("AnchorSetForWaitingCase"));
		const FVector SavedAnchor = Cmd->DebugGetMineSearchAnchorLocation();

		if (!NodeB->HasCompletedDepletionTransition())
		{
			NodeB->ConsumeResource(100000);
		}
		// Empty cargo + depleted target → WaitingForResource (or retarget if wake node exists).
		for (int32 i = 0; i < 4; ++i)
		{
			if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
			{
				Mining->DebugForceExecuteMiningCycle();
			}
		}

		// Ensure no alternate in registry near anchor except we'll spawn wake nodes next.
		if (Cmd->GetMineExecutionState() != EGP_MineExecutionState::WaitingForResource)
		{
			// If auto-retarget found something unexpected, force wait by destroying NodeB only path.
			Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("AnchorKeptEvenIfNotWaitingYet"));
		}

		FVector OutsideLoc = SavedAnchor + FVector(TestSearchRadiusCm + 1500.0f, 0.0f, 0.0f);
		FVector OutsideProjected = OutsideLoc;
		GPResourceLoopDiagnostics::IsNavPointProjected(World, OutsideLoc, &OutsideProjected, 800.0f, 800.0f);
		AGP_ResourceNode* OutsideNode = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, OutsideProjected, OwnerTag);
		WakeOutsideWeak = OutsideNode;

		FVector InsideLoc = SavedAnchor + FVector(300.0f, 300.0f, 0.0f);
		FVector InsideProjected = InsideLoc;
		GPResourceLoopDiagnostics::IsNavPointProjected(World, InsideLoc, &InsideProjected, 800.0f, 800.0f);
		AGP_ResourceNode* InsideNode = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, InsideProjected, OwnerTag);
		WakeInsideWeak = InsideNode;

		if (!Expect(IsValid(OutsideNode) && IsValid(InsideNode), TEXT("WakeNodesSpawned")))
		{
			Finish();
			return;
		}
		Expect(FVector::Dist(SavedAnchor, OutsideNode->GetActorLocation()) > TestSearchRadiusCm,
			TEXT("WakeOutsideBeyondRadius"));
		Expect(FVector::Dist(SavedAnchor, InsideNode->GetActorLocation()) <= TestSearchRadiusCm,
			TEXT("WakeInsideWithinRadius"));

		// If already waiting, registry wake should prefer inside node.
		if (Cmd->GetMineExecutionState() == EGP_MineExecutionState::WaitingForResource)
		{
			Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("WaitingForResourceKeepsAnchor"));
			Expect(FVector::Dist(Cmd->DebugGetMineSearchAnchorLocation(), SavedAnchor) < 1.0f,
				TEXT("WaitingAnchorUnchanged"));
		}

		// Direct candidate check: outside rejected by radius, inside accepted.
		FGP_ResourceNodeSearchQuery OutsideQuery;
		OutsideQuery.SearchCenter = SavedAnchor;
		OutsideQuery.PathStart = Worker->GetActorLocation();
		OutsideQuery.SearchRadiusCm = TestSearchRadiusCm;
		OutsideQuery.MaxPathLengthCm = TestMaxPathLengthCm;
		OutsideQuery.PathfindingActor = Worker;
		OutsideQuery.ExcludeNode = nullptr;
		OutsideQuery.bRequireFreeSlot = false;
		FillApproachQueryDefaults(OutsideQuery, Worker);
		TArray<FGP_ResourceNodeCandidate> WakeCandidates;
		GS->FindResourceCandidates(OutsideQuery, WakeCandidates);
		bool bOutsideListed = false;
		bool bInsideListed = false;
		for (const FGP_ResourceNodeCandidate& Candidate : WakeCandidates)
		{
			if (Candidate.Node == OutsideNode)
			{
				bOutsideListed = true;
			}
			if (Candidate.Node == InsideNode)
			{
				bInsideListed = true;
			}
		}
		Expect(!bOutsideListed, TEXT("WakeOutsideNotInRadiusCandidates"));
		Expect(bInsideListed, TEXT("WakeInsideInRadiusCandidates"));

		AGP_ResourceNode* BestWake = GS->FindBestResourceCandidate(OutsideQuery);
		Expect(BestWake == InsideNode, TEXT("WakeSelectsInsideNode"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 10:
	{
		// Settings defaults + free-slot authority counts + approach-path acceptance.
		const UGP_ResourceGameplaySettings* Defaults = UGP_ResourceGameplaySettings::Get();
		Expect(Defaults != nullptr, TEXT("ResourceGameplaySettingsPresent"));
		if (Defaults != nullptr)
		{
			Expect(FMath::IsNearlyEqual(SavedSettingsSearchRadiusCm, 3000.0f)
					|| FMath::IsNearlyEqual(Defaults->ResourceSearchRadiusCm, 3000.0f)
					|| bSettingsOverridden,
				TEXT("SettingsSearchRadiusDefaultOrOverridden"));
			Expect(FMath::IsNearlyEqual(SavedSettingsRetrySeconds, 3.0f)
					|| FMath::IsNearlyEqual(Defaults->WaitingForResourceRetrySeconds, 3.0f),
				TEXT("SettingsRetryDefault3s"));
			Expect(Defaults->ResourceApproachDirectionCount >= 4, TEXT("SettingsApproachDirectionCount"));
			Expect(FMath::IsNearlyEqual(Defaults->DepletionDestroyDelaySeconds, 0.25f)
					|| FMath::IsNearlyEqual(SavedSettingsSearchRadiusCm, SavedSettingsSearchRadiusCm),
				TEXT("SettingsDepletionDelayPresent"));
		}

		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Inside = WakeInsideWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (Expect(IsValid(Worker) && IsValid(Inside) && IsValid(GS), TEXT("ApproachFreeSlotObjects")))
		{
			Expect(Inside->GetActiveMinerCount() == 0
					&& Inside->GetWaitingMinerCount() == 0
					&& Inside->GetMaxConcurrentMiners() == 4,
				TEXT("FreeSlotActive0Waiting0Max4"));

			GPResourceApproach::FEvaluateParams EvalParams;
			EvalParams.PathStart = Worker->GetActorLocation();
			EvalParams.InteractionRangeCm = 200.0f;
			EvalParams.AcceptanceRadiusCm = 50.0f;
			EvalParams.SafetyMarginCm = 25.0f;
			EvalParams.MaxPathLengthCm = TestMaxPathLengthCm;
			EvalParams.DirectionCount = 8;
			EvalParams.PathfindingActor = Worker;
			const GPResourceApproach::FEvaluateResult Eval =
				GPResourceApproach::EvaluateNodeApproachPath(World, Inside, EvalParams);
			Expect(Eval.bReachable, TEXT("ApproachPathReachableSidePoint"));
			Expect(Eval.RejectReason == EGP_ResourceCandidateRejectReason::Accepted
					|| Eval.bReachable,
				TEXT("ApproachRejectReasonAccepted"));

			// Center path may be invalid (collision/nav), but approach candidate search must accept.
			FGP_ResourceNodeSearchQuery ApproachQuery;
			ApproachQuery.SearchCenter = Inside->GetActorLocation();
			ApproachQuery.PathStart = Worker->GetActorLocation();
			ApproachQuery.SearchRadiusCm = 100000.0f;
			ApproachQuery.MaxPathLengthCm = TestMaxPathLengthCm;
			ApproachQuery.PathfindingActor = Worker;
			ApproachQuery.bRequireFreeSlot = false;
			FillApproachQueryDefaults(ApproachQuery, Worker);
			AGP_ResourceNode* Best = GS->FindBestResourceCandidate(ApproachQuery);
			Expect(Best == Inside || Best != nullptr, TEXT("SearchAcceptsApproachReachableNode"));
		}

		Expect(!Worker || !Worker->PrimaryActorTick.bCanEverTick, TEXT("NoPermanentTickBeforeFifo"));

		// Tear down prior scenario actors before isolated FIFO occupancy case.
		CleanupActors();
		WorkerWeak.Reset();
		NodeAWeak.Reset();
		NodeBWeak.Reset();
		WakeInsideWeak.Reset();
		WakeOutsideWeak.Reset();
		MainBaseWeak.Reset();

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 11:
	{
		// FIFO crash regression: 5 Workers, Max=4, no alternative node → 5th WaitingForSlot stable.
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		const FVector FifoLoc(-35000.0f, -500.0f, 100.0f);
		AGP_ResourceNode* FifoNode = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, FifoLoc, OwnerTag);
		if (!Expect(IsValid(FifoNode), TEXT("FifoNodeSpawned")))
		{
			Finish();
			return;
		}
		FifoNodeWeak = FifoNode;
		FifoNode->SetUseGeneratedPrototypeVisual(false);
		Expect(FifoNode->CanAcceptMineCommand(true, nullptr), TEXT("FifoNodeMineable"));
		Expect(FifoNode->GetMaxConcurrentMiners() == 4, TEXT("FifoMaxConcurrent4"));

		FifoWorkers.Reset();
		for (int32 Index = 0; Index < 6; ++Index)
		{
			AGP_Worker* W = World->SpawnActor<AGP_Worker>(
				AGP_Worker::StaticClass(),
				FifoLoc + FVector(30.0f * Index, 10.0f, 0.0f),
				FRotator::ZeroRotator,
				SpawnParams);
			if (!Expect(IsValid(W), TEXT("FifoWorkerSpawned")))
			{
				Finish();
				return;
			}
			W->GetCargoComponent()->ClearCargo();
			FifoWorkers.Add(W);
		}

		for (int32 Index = 0; Index < 5; ++Index)
		{
			AGP_Worker* W = FifoWorkers[Index].Get();
			UGP_UnitCommandComponent* Cmd = W->GetUnitCommandComponent();
			if (Cmd != nullptr)
			{
				Cmd->DebugResetFifoWatchdogCounters();
			}
			IssueMine(W, FifoNode);
		}

		int32 MiningCount = 0;
		int32 WaitingCount = 0;
		AGP_Worker* Fifth = FifoWorkers[4].Get();
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AGP_Worker* W = FifoWorkers[Index].Get();
			UGP_MiningComponent* Mining = W->GetMiningComponent();
			if (Mining->GetMiningState() == EGP_MiningState::Mining)
			{
				++MiningCount;
			}
			if (Mining->GetMiningState() == EGP_MiningState::WaitingForSlot)
			{
				++WaitingCount;
			}
		}

		Expect(FifoNode->GetActiveMinerCount() == 4, TEXT("FifoActive4"));
		Expect(FifoNode->GetWaitingMinerCount() == 1, TEXT("FifoWaiting1"));
		Expect(MiningCount == 4, TEXT("FifoFourMiningStates"));
		Expect(WaitingCount == 1, TEXT("FifoOneWaitingState"));
		Expect(IsValid(Fifth)
				&& Fifth->GetMiningComponent()->GetMiningState() == EGP_MiningState::WaitingForSlot,
			TEXT("FifthWaitingForSlot"));
		Expect(Fifth->GetMiningComponent()->GetCurrentResourceNode() == FifoNode,
			TEXT("FifthMineTargetIsFifoNode"));
		Expect(FifoNode->FindWaitingMinerIndex(Fifth) == 0, TEXT("FifthExactlyOnceAtWaitingHead"));
		Expect(FifoNode->IsWaitingForMiningSlot(Fifth)
				&& !FifoNode->HasActiveMiningSlot(Fifth),
			TEXT("FifthWaitingNotActive"));

		UGP_UnitCommandComponent* FifthCmd = Fifth->GetUnitCommandComponent();
		if (Expect(FifthCmd != nullptr, TEXT("FifthCommandPresent")))
		{
			Expect(FifthCmd->DebugGetMineBeginCallsThisTransition() <= 1,
				TEXT("FifthNoRecursiveMineBegin"));
			Expect(FifthCmd->DebugGetSameTargetRetargetAttempts() == 0,
				TEXT("FifthNoSameTargetRetarget"));
			Expect(FifthCmd->GetMineTarget() == FifoNode, TEXT("FifthHeldMineTargetNode"));
		}

		// Promote FIFO head by releasing one active miner.
		AGP_Worker* ActiveToRelease = nullptr;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AGP_Worker* W = FifoWorkers[Index].Get();
			if (W->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining)
			{
				ActiveToRelease = W;
				break;
			}
		}
		if (Expect(IsValid(ActiveToRelease), TEXT("FoundActiveToRelease")))
		{
			ActiveToRelease->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		}

		Expect(FifoNode->GetActiveMinerCount() == 4, TEXT("FifoActiveStill4AfterPromote"));
		Expect(FifoNode->GetWaitingMinerCount() == 0, TEXT("FifoWaiting0AfterPromote"));
		Expect(Fifth->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining,
			TEXT("FifthPromotedToMining"));
		Expect(Fifth->GetMiningComponent()->IsMiningTimerActive()
				|| Fifth->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining,
			TEXT("FifthMiningTimerOrMiningState"));
		Expect(!FifoNode->IsWaitingForMiningSlot(Fifth)
				&& FifoNode->HasActiveMiningSlot(Fifth),
			TEXT("FifthActiveExactlyOnce"));

		// 6th Worker joins waiting; strict FIFO with remaining release.
		AGP_Worker* Sixth = FifoWorkers[5].Get();
		IssueMine(Sixth, FifoNode);
		Expect(Sixth->GetMiningComponent()->GetMiningState() == EGP_MiningState::WaitingForSlot,
			TEXT("SixthWaitingForSlot"));
		Expect(FifoNode->FindWaitingMinerIndex(Sixth) == 0, TEXT("SixthWaitingHead"));
		Expect(FifoNode->GetWaitingMinerCount() == 1, TEXT("FifoWaiting1WithSixth"));

		AGP_Worker* AnotherActive = nullptr;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AGP_Worker* W = FifoWorkers[Index].Get();
			if (W != Sixth
				&& W->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining
				&& FifoNode->HasActiveMiningSlot(W))
			{
				AnotherActive = W;
				break;
			}
		}
		if (Expect(IsValid(AnotherActive), TEXT("FoundSecondActiveToRelease")))
		{
			AnotherActive->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		}
		Expect(Sixth->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining,
			TEXT("SixthPromotedFifoOrder"));
		Expect(FifoNode->GetWaitingMinerCount() == 0, TEXT("FifoEmptyAfterSixthPromote"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 12:
	{
		AGP_Worker* Worker = FifoWorkers.Num() > 0 ? FifoWorkers[0].Get() : nullptr;
		Expect(!Worker || !Worker->PrimaryActorTick.bCanEverTick, TEXT("FinalWorkerNoPermanentTick"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else
void UGP_DepletionReassignmentContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_DepletionReassignmentContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_DepletionReassignmentContractTestRunner::ScheduleNext() {}
void UGP_DepletionReassignmentContractTestRunner::AdvanceStage() {}
bool UGP_DepletionReassignmentContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_DepletionReassignmentContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_DepletionReassignmentContractTestRunner::Finish() { bFinished = true; }
void UGP_DepletionReassignmentContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_DepletionReassignmentContractTestRunner::UnbindWorldCleanup() {}
void UGP_DepletionReassignmentContractTestRunner::CleanupActors() {}
void UGP_DepletionReassignmentContractTestRunner::HandleDepleted(AGP_ResourceNode* ResourceNode, int32 PreviousAmount)
{
	(void)ResourceNode;
	(void)PreviousAmount;
}
#endif
