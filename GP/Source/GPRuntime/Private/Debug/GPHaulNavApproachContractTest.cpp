// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPHaulNavApproachContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Components/BoxComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "NavAreas/NavArea_Null.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHaulNavApproach, Log, All);

namespace GPHaulNavApproachDebug
{
	static TWeakObjectPtr<UGP_HaulNavApproachContractTestRunner> GActiveRunner;

	static constexpr int32 SkipDirectRadialCandidateMask = 1 << 0;
	static constexpr int32 SkipAllCandidatesMask = 0xFF;

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

	static void FillCargoFull(AGP_Worker* Worker)
	{
		if (!IsValid(Worker) || !IsValid(Worker->GetMiningComponent()) || !IsValid(Worker->GetCargoComponent()))
		{
			return;
		}
		for (int32 i = 0; i < 12; ++i)
		{
			if (Worker->GetCargoComponent()->IsFull())
			{
				break;
			}
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
	}

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
		NavBox->SetBoxExtent(FVector(160.0f, 160.0f, 130.0f));
		NavBox->SetCanEverAffectNavigation(true);
		NavBox->bDynamicObstacle = true;
		NavBox->SetAreaClassOverride(UNavArea_Null::StaticClass());
		NavBox->UpdateBounds();
		NavBox->MarkRenderStateDirty();
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

	static bool DestinationOutsideNavObstacle(const AGP_MainBase* Base, const FVector& Dest)
	{
		if (!IsValid(Base))
		{
			return false;
		}
		const UBoxComponent* NavBox = Base->GetNavigationObstacle();
		if (NavBox == nullptr)
		{
			return true;
		}
		const FVector Extent = NavBox->Bounds.BoxExtent;
		const float MinOutside = FMath::Max(Extent.X, Extent.Y) + 5.0f;
		return FVector::Dist2D(Dest, Base->GetActorLocation()) + KINDA_SMALL_NUMBER >= MinOutside;
	}

	static void RunHaulNavApproachContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPHaulNavApproach, Warning,
				TEXT("gp.Resource.RunHaulNavApproachContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("HaulNavApproachContract"), TEXT("HaulNavApproach"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPHaulNavApproach, Warning,
				TEXT("gp.Resource.RunHaulNavApproachContractTest: already running"));
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_HaulNavApproachContractTestRunner* Runner =
			NewObject<UGP_HaulNavApproachContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GHaulNavApproachContract(
		TEXT("gp.Resource.RunHaulNavApproachContractTest"),
		TEXT("GP-S33M haul nav-aware approach around MainBase NavigationObstacle"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunHaulNavApproachContractTest));
}

void UGP_HaulNavApproachContractTestRunner::BeginDestroy()
{
	if (!bFinished)
	{
		bCancelled = true;
		CancelReason = TEXT("BeginDestroy");
		Finish();
	}
	Super::BeginDestroy();
}

void UGP_HaulNavApproachContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_HaulNavApproachContractTestRunner::OnWorldCleanup(
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

void UGP_HaulNavApproachContractTestRunner::CleanupActors()
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
	DestroyWeak(NodeWeak);
	DestroyWeak(MainBaseWeak);
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
}

void UGP_HaulNavApproachContractTestRunner::Finish()
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

	UE_LOG(LogGPHaulNavApproach, Log,
		TEXT("gp.Resource.RunHaulNavApproachContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPHaulNavApproachDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_HaulNavApproachContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPHaulNavApproach, Error,
		TEXT("gp.Resource.RunHaulNavApproachContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_HaulNavApproachContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPHaulNavApproach, Error,
			TEXT("gp.Resource.RunHaulNavApproachContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPHaulNavApproach, Log,
			TEXT("gp.Resource.RunHaulNavApproachContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_HaulNavApproachContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	if (DelaySeconds <= 0.0f)
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UGP_HaulNavApproachContractTestRunner::AdvanceStage));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_HaulNavApproachContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_HaulNavApproachContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	MovementWaitTicks = 0;
	SelectedCandidateIndex = -1;
	SelectedDestination = FVector::ZeroVector;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_HaulNavApproachContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPHaulNavApproach, Log, TEXT("gp.Resource.RunHaulNavApproachContractTest Start"));
	ScheduleNext(0.05f);
}

void UGP_HaulNavApproachContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	using namespace GPHaulNavApproachDebug;

	auto WaitCondition = [&](bool bReady, const TCHAR* TimeoutLabel) -> bool
	{
		if (bReady)
		{
			MovementWaitTicks = 0;
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
		// Spawn MainBase + Worker + deposit; activate NavigationObstacle; skip radial candidate.
		FVector Anchor = FVector(0.0f, 0.0f, 100.0f);
		FVector Projected;
		if (GPResourceLoopDiagnostics::IsNavPointProjected(World, Anchor, &Projected, 2000.0f, 2000.0f))
		{
			Anchor = Projected;
		}
		const int32 FreeTeam = GPResourceLoopDiagnostics::FindFreePlayableTeamId(World);
		ContractTeamId = FreeTeam >= 1 ? FreeTeam : 1;

		MainBaseLocation = Anchor + FVector(-700.0f, 0.0f, 0.0f);
		WorkerSpawnLocation = Anchor + FVector(350.0f, 80.0f, 0.0f);
		const FVector NodeLoc = Anchor + FVector(420.0f, 80.0f, 0.0f);

		AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, MainBaseLocation, ContractTeamId, OwnerTag);
		AGP_ResourceNode* Node = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
			World, NodeLoc, OwnerTag);
		AGP_Worker* Worker = GPResourceLoopDiagnostics::SpawnWorkerDeferred(
			World, WorkerSpawnLocation, ContractTeamId, OwnerTag);

		if (!Expect(IsValid(Base) && IsValid(Node) && IsValid(Worker), TEXT("SpawnMainBaseWorkerNode")))
		{
			Finish();
			return;
		}

		ActivateMainBaseNavigationObstacle(Base);
		Expect(Base->GetNavigationObstacle() != nullptr
				&& Base->GetNavigationObstacle()->CanEverAffectNavigation(),
			TEXT("A_MainBaseNavObstacleActive"));

		MainBaseWeak = Base;
		NodeWeak = Node;
		WorkerWeak = Worker;
		Node->DebugSetCurrentAmountForTest(5000, false);
		Worker->GetCargoComponent()->ClearCargo();

		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (!Expect(Cmd != nullptr, TEXT("WorkerHasUnitCommand")))
		{
			Finish();
			return;
		}
		Cmd->DebugSetApproachSkipCandidateMask(SkipDirectRadialCandidateMask);

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		// Mine → force CargoFull → haul must pick alternate candidate (not index 0).
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Node) && IsValid(Base) && Cmd && Mining,
				TEXT("HaulObjects")))
		{
			Finish();
			return;
		}

		if (!Cmd->HasHeldCommand()
			|| Cmd->GetHeldCommand()->CommandTag != FGPGameplayTags::Get().Command_Mine)
		{
			IssueMine(Worker, Node);
		}

		if (!HaulActiveTowardBase(Cmd) && !Worker->GetCargoComponent()->IsFull())
		{
			if (Mining->GetCurrentResourceNode() != Node
				|| Mining->GetMiningState() != EGP_MiningState::Mining)
			{
				const bool bReady =
					Mining->GetCurrentResourceNode() == Node
					&& Mining->GetMiningState() == EGP_MiningState::Mining;
				if (WaitCondition(bReady, TEXT("BeginMiningTimeout")))
				{
					return;
				}
			}
			FillCargoFull(Worker);
		}

		if (WaitCondition(HaulActiveTowardBase(Cmd), TEXT("B_HaulStarted_AlternatePathTimeout")))
		{
			return;
		}

		SelectedCandidateIndex = Cmd->DebugGetLastApproachCandidateIndex();
		SelectedDestination = Cmd->GetHaulApproachDestination();

		// A: direct radial skipped/unavailable; B: alternate selected.
		Expect(SelectedCandidateIndex > 0, TEXT("A_DirectCandidateSkipped_B_AlternateSelected"));
		Expect(SelectedCandidateIndex != 0, TEXT("B_NotRadialIndex0"));

		const float DropOff = Base->GetDropOffRangeCm();
		Expect(FVector::Dist(SelectedDestination, Base->GetActorLocation()) <= DropOff + KINDA_SMALL_NUMBER,
			TEXT("C_DestinationInsideDropOffRange"));
		Expect(DestinationOutsideNavObstacle(Base, SelectedDestination),
			TEXT("C_DestinationOutsideNavObstacleFootprint"));
		Expect(!SelectedDestination.IsNearlyZero(), TEXT("C_DestinationNonZero"));

		// Soft assertion that we did not route to MainBase center (would be inside Null footprint).
		Expect(FVector::Dist2D(SelectedDestination, Base->GetActorLocation()) > 50.0f,
			TEXT("E_NoCenterStraightThroughObstacle"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.1f);
		break;
	}
	case 2:
	{
		// Reach drop-off range and unload.
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
		if (WaitCondition(bUnloaded, TEXT("C_UnloadAfterAlternateApproachTimeout")))
		{
			return;
		}

		Expect(bUnloaded, TEXT("C_UnloadSucceeded"));
		Expect(Cargo->IsEmpty(), TEXT("C_CargoEmpty"));

		++StageIndex;
		MovementWaitTicks = 0;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		// D: all candidates unreachable → WaitingForDropOff (PathRejected), no infinite spam.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_UnitCommandComponent* Cmd = IsValid(Worker) ? Worker->GetUnitCommandComponent() : nullptr;
		UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		UGP_MiningComponent* Mining = IsValid(Worker) ? Worker->GetMiningComponent() : nullptr;
		if (!Expect(IsValid(Worker) && IsValid(Node) && IsValid(Base) && Cmd && Cargo && Mining,
				TEXT("AllUnreachableObjects")))
		{
			Finish();
			return;
		}

		Cmd->DebugSetApproachSkipCandidateMask(SkipAllCandidatesMask);
		Cargo->ClearCargo();
		Worker->SetActorLocation(WorkerSpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		if (Mining->GetCurrentResourceNode() != Node)
		{
			Mining->BeginMining(Node);
		}
		FillCargoFull(Worker);

		const bool bWaiting = Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::WaitingForDropOff;
		if (WaitCondition(bWaiting, TEXT("D_AllCandidatesUnreachable_WaitingForDropOffTimeout")))
		{
			return;
		}

		Expect(bWaiting, TEXT("D_WaitingForDropOff"));
		Expect(Cargo->IsFull() || Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER,
			TEXT("D_CargoRetained"));
		Expect(Cmd->DebugIsDropOffRetryArmed() || bWaiting, TEXT("D_RetryArmedOrWaiting"));
		// E: no accepted approach move — WaitingForDropOff, not ReturningToBase through Null.
		Expect(!HaulActiveTowardBase(Cmd), TEXT("E_NoAcceptedMoveThroughNavAreaNull"));
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff,
			TEXT("E_PathRejectedWaitNotStraightFallback"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_HaulNavApproachContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_HaulNavApproachContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_HaulNavApproachContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_HaulNavApproachContractTestRunner::AdvanceStage() {}
bool UGP_HaulNavApproachContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_HaulNavApproachContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_HaulNavApproachContractTestRunner::Finish() { bFinished = true; }
void UGP_HaulNavApproachContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_HaulNavApproachContractTestRunner::UnbindWorldCleanup() {}
void UGP_HaulNavApproachContractTestRunner::CleanupActors() {}

#endif
