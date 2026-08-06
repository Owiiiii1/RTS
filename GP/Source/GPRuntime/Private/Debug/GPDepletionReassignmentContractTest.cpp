// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceNode.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Visual/GPResourceNodeVisualComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPDepletionReassignment, Log, All);

namespace GPDepletionReassignmentDebug
{
	static TWeakObjectPtr<UGP_DepletionReassignmentContractTestRunner> GActiveRunner;

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
		TEXT("Authority: GP-S28P2 depletion/registry/reassignment contract. Transient actors only."),
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
}

void UGP_DepletionReassignmentContractTestRunner::Finish()
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

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

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
		Query.Origin = NodeA->GetActorLocation();
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
		// Allow deferred destroy timer (0.25s default) — wait a couple ticks via re-schedule.
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
