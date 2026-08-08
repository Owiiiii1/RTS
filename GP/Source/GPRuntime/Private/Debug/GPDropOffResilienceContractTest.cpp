// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPDropOffResilience, Log, All);

namespace GPDropOffResilienceDebug
{
	static TWeakObjectPtr<UGP_DropOffResilienceContractTestRunner> GActiveRunner;

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

	static void FillCargoFull(AGP_Worker* Worker)
	{
		if (!IsValid(Worker) || !IsValid(Worker->GetMiningComponent()) || !IsValid(Worker->GetCargoComponent()))
		{
			return;
		}
		for (int32 i = 0; i < 8; ++i)
		{
			if (Worker->GetCargoComponent()->IsFull())
			{
				break;
			}
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
	}

	static void RunDropOffResilienceContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPDropOffResilience, Warning,
				TEXT("GP Resource.RunDropOffResilienceContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("DropOffResilienceContract"), TEXT("DropOffResilience"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_DropOffResilienceContractTestRunner* Runner =
			NewObject<UGP_DropOffResilienceContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GDropOffResilienceContract(
		TEXT("gp.Resource.RunDropOffResilienceContractTest"),
		TEXT("Authority: GP-S28P3 WaitingForDropOff / MainBase wake / destroy / unreachable contract. Transient only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunDropOffResilienceContractTest));
}

void UGP_DropOffResilienceContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_DropOffResilienceContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_DropOffResilienceContractTestRunner::OnWorldCleanup(
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

void UGP_DropOffResilienceContractTestRunner::CleanupActors()
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

void UGP_DropOffResilienceContractTestRunner::Finish()
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
			Settings->DropOffRetrySeconds = SavedDropOffRetrySeconds;
		}
		bSettingsOverridden = false;
	}

	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	UnbindWorldCleanup();
	CleanupActors();

	UE_LOG(LogGPDropOffResilience, Log,
		TEXT("GP Resource.RunDropOffResilienceContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPDropOffResilienceDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_DropOffResilienceContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPDropOffResilience, Error,
		TEXT("GP Resource.RunDropOffResilienceContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_DropOffResilienceContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPDropOffResilience, Error,
			TEXT("GP Resource.RunDropOffResilienceContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPDropOffResilience, Log,
			TEXT("GP Resource.RunDropOffResilienceContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_DropOffResilienceContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_DropOffResilienceContractTestRunner::AdvanceStage));
}

void UGP_DropOffResilienceContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	StableWaitTicks = 0;
	MovementWaitTicks = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_DropOffResilienceContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPDropOffResilience, Log, TEXT("GP Resource.RunDropOffResilienceContractTest Start"));
	ScheduleNext();
}

void UGP_DropOffResilienceContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	using namespace GPDropOffResilienceDebug;

	auto WaitHaulOrMove = [&](AGP_Worker* Worker, const TCHAR* TimeoutLabel) -> bool
	{
		if (!IsValid(Worker))
		{
			Expect(false, TEXT("WaitHaulWorkerLost"));
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

	auto WaitUntilHaulState = [&](AGP_Worker* Worker, EGP_HaulExecutionState Wanted, const TCHAR* TimeoutLabel) -> bool
	{
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("WaitHaulStateWorkerLost"));
			Finish();
			return true;
		}
		if (Worker->GetUnitCommandComponent()->GetHaulExecutionState() == Wanted)
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
		// Operator helper path: Spawn (BeginPlay TeamId=-1) → SetTeamId → register once → destroy → unregister once.
		{
			AGP_GameState* GS = World->GetGameState<AGP_GameState>();
			const int32 FreeTeam = GPResourceLoopDiagnostics::FindFreePlayableTeamId(World);
			if (!Expect(IsValid(GS) && FreeTeam >= 1, TEXT("HelperRegistryFreeTeam")))
			{
				Finish();
				return;
			}

			int32 RegisterCount = 0;
			int32 UnregisterCount = 0;
			AGP_MainBase* ObservedRegister = nullptr;
			FString ObservedUnregisterName;
			const FDelegateHandle RegHandle = GS->OnMainBaseRegistered.AddLambda(
				[&RegisterCount, &ObservedRegister](AGP_MainBase* MainBase)
				{
					if (IsValid(MainBase))
					{
						++RegisterCount;
						ObservedRegister = MainBase;
					}
				});
			const FDelegateHandle UnregHandle = GS->OnMainBaseUnregistered.AddLambda(
				[&UnregisterCount, &ObservedUnregisterName](AGP_MainBase* MainBase)
				{
					if (MainBase != nullptr)
					{
						++UnregisterCount;
						ObservedUnregisterName = GetNameSafe(MainBase);
					}
				});

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			AGP_MainBase* HelperBase = World->SpawnActor<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				FVector(120.0f, 120.0f, 100.0f),
				FRotator::ZeroRotator,
				Params);
			if (!Expect(IsValid(HelperBase), TEXT("HelperRegistrySpawn")))
			{
				GS->OnMainBaseRegistered.Remove(RegHandle);
				GS->OnMainBaseUnregistered.Remove(UnregHandle);
				Finish();
				return;
			}
			Expect(HelperBase->GetTeamId() < 1, TEXT("HelperRegistryBeginPlayUnassigned"));
			Expect(GS->FindMainBaseForTeam(FreeTeam) != HelperBase, TEXT("HelperRegistryNotRegisteredBeforeSetTeamId"));

			HelperBase->SetTeamId(FreeTeam);
			Expect(HelperBase->GetTeamId() == FreeTeam, TEXT("HelperRegistryTeamIdApplied"));
			Expect(GS->FindMainBaseForTeam(FreeTeam) == HelperBase, TEXT("HelperRegistryFindAfterSetTeamId"));
			Expect(RegisterCount == 1, TEXT("HelperRegistryRegisteredOnce"));
			Expect(ObservedRegister == HelperBase, TEXT("HelperRegistryRegisterEventActor"));

			const FString HelperName = GetNameSafe(HelperBase);
			HelperBase->Destroy();
			Expect(GS->FindMainBaseForTeam(FreeTeam) == nullptr, TEXT("HelperRegistryClearedAfterDestroy"));
			Expect(UnregisterCount == 1, TEXT("HelperRegistryUnregisteredOnce"));
			Expect(ObservedUnregisterName == HelperName, TEXT("HelperRegistryUnregisterEventActor"));

			GS->OnMainBaseRegistered.Remove(RegHandle);
			GS->OnMainBaseUnregistered.Remove(UnregHandle);
		}

		// Case 1 setup: navigable scenario then remove MainBase before cargo-full haul.
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("SpawnNavigableScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		ScenarioNodeLocation = Scenario.ResourceNodeLocation;
		ThreatBefore = 0.0f;
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			ThreatBefore = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
		}
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
			MainBaseWeak.Reset();
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		// Case 1 — missing MainBase → WaitingForDropOff; cargo + held Mine retained.
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("Case1Objects")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		FillCargoFull(Worker);
		CargoAtWait = Worker->GetCargoComponent()->GetCurrentCargoAmount();
		Expect(CargoAtWait > KINDA_SMALL_NUMBER, TEXT("Case1CargoPresent"));
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case1WaitingForDropOff"));
		Expect(Worker->GetWorkerActivityState() == EGP_WorkerActivityState::WaitingForDropOff, TEXT("Case1ActivityWaiting"));
		Expect(Cmd->HasHeldCommand(), TEXT("Case1HeldMineRetained"));
		Expect(Cmd->DebugHasMineSearchAnchor(), TEXT("Case1SearchAnchorRetained"));
		Expect(Cmd->DebugIsWaitingRegisterBound(), TEXT("Case1RegisterWakeBound"));
		Expect(!Cmd->DebugIsActiveHaulUnregisterBound(), TEXT("Case1NoActiveUnregister"));
		Expect(Cmd->DebugIsDropOffRetryArmed(), TEXT("Case1RetryArmed"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), CargoAtWait), TEXT("Case1CargoUnchanged"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			Expect(FMath::IsNearlyEqual(GS->GetFerroniteThreatValueForTeam(ContractTeamId), ThreatBefore), TEXT("Case1ThreatUnchanged"));
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		// Case 2 — register MainBase wake once → haul → unload.
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(Worker), TEXT("Case2Worker")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		const int32 WakeBefore = Cmd->DebugGetDropOffWakeCount();
		AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ScenarioBaseLocation, ContractTeamId, OwnerTag);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("Case2SpawnMainBase")))
		{
			Finish();
			return;
		}
		Expect(Cmd->DebugGetDropOffWakeCount() >= WakeBefore + 1 || Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::WaitingForDropOff,
			TEXT("Case2WakeObservedOrLeftWait"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("Case2ObjectsAfterHaul"));
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff)
		{
			if (MovementWaitTicks == 0)
			{
				MovementWaitStartTime = World->GetTimeSeconds();
			}
			++MovementWaitTicks;
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("Case2WakeResumeTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (WaitHaulOrMove(Worker, TEXT("Case2HaulTimeout")))
		{
			return;
		}
		if (!Expect(IsValid(Worker) && IsValid(Base), TEXT("Case2ObjectsAfterHaul")))
		{
			Finish();
			return;
		}
		Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->DebugGetDropOffWakeCount() == 1, TEXT("Case2WakeExactlyOnce"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("Case2CargoUnloaded"));
		Expect(Base->GetStorageComponent()->GetTotalStored() >= CargoAtWait - 0.5f, TEXT("Case2StorageReceived"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
			Expect(ThreatAfter > ThreatBefore + KINDA_SMALL_NUMBER, TEXT("Case2ThreatIncreasedAfterAccept"));
		}
		Expect(!Cmd->DebugIsWaitingRegisterBound(), TEXT("Case2RegisterUnboundAfterDeliver"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		// Case 3 — destroy current target mid-haul (ReturningToBase).
		CleanupActors();
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("Case3SpawnScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		ScenarioNodeLocation = Scenario.ResourceNodeLocation;
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		FillCargoFull(Worker);
		CargoAtWait = Worker->GetCargoComponent()->GetCurrentCargoAmount();
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("Case3WorkerLost"));
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		const EGP_HaulExecutionState Haul = Cmd->GetHaulExecutionState();
		const bool bActiveHaulLeg =
			Haul == EGP_HaulExecutionState::ReturningToBase
			|| Haul == EGP_HaulExecutionState::DroppingOff;
		if (!bActiveHaulLeg)
		{
			if (MovementWaitTicks == 0)
			{
				MovementWaitStartTime = World->GetTimeSeconds();
			}
			++MovementWaitTicks;
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("Case3ActiveHaulTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		Expect(Cmd->DebugIsActiveHaulUnregisterBound(), TEXT("Case3ActiveUnregisterBound"));
		Expect(!Cmd->DebugIsWaitingRegisterBound(), TEXT("Case3NoRegisterWhileHauling"));
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(Base), TEXT("Case3BaseBeforeDestroy")))
		{
			Finish();
			return;
		}
		Base->Destroy();
		MainBaseWeak.Reset();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case3WaitingImmediate"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), CargoAtWait), TEXT("Case3CargoUnchanged"));
		Expect(Cmd->HasHeldCommand(), TEXT("Case3HeldRetained"));
		Expect(Cmd->DebugIsWaitingRegisterBound(), TEXT("Case3RegisterBoundAfterDestroy"));
		Expect(!Cmd->DebugIsActiveHaulUnregisterBound(), TEXT("Case3UnregisterCleared"));
		Expect(!(Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving()), TEXT("Case3MovementCancelled"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		// Case 4 — replacement MainBase wake once → deliver.
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(Worker), TEXT("Case4Worker")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Cmd->DebugGetDropOffWakeCount(); // ensure API linked
		// Reset wake counter expectation relative to current (should already be 0 after ResetHaul on prior deliver; mid-destroy path didn't reset).
		const int32 WakeBefore = Cmd->DebugGetDropOffWakeCount();
		AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ScenarioBaseLocation, ContractTeamId, OwnerTag);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("Case4SpawnReplacement")))
		{
			Finish();
			return;
		}
		Expect(Cmd->DebugGetDropOffWakeCount() == WakeBefore + 1
				|| Cmd->GetHaulExecutionState() != EGP_HaulExecutionState::WaitingForDropOff,
			TEXT("Case4WakeOnce"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 7:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("Case4Objects"));
			Finish();
			return;
		}
		if (Worker->GetUnitCommandComponent()->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff)
		{
			if (MovementWaitTicks == 0)
			{
				MovementWaitStartTime = World->GetTimeSeconds();
			}
			++MovementWaitTicks;
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("Case4WakeResumeTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (WaitHaulOrMove(Worker, TEXT("Case4HaulTimeout")))
		{
			return;
		}
		if (!Expect(IsValid(Worker) && IsValid(Base), TEXT("Case4Objects")))
		{
			Finish();
			return;
		}
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("Case4Delivered"));
		Expect(Base->GetStorageComponent()->GetTotalStored() >= CargoAtWait - 0.5f, TEXT("Case4Storage"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 8:
	{
		// Case 5 — unreachable (forced approach reject) → stable WaitingForDropOff; resume via retry.
		CleanupActors();
		if (UGP_ResourceGameplaySettings* Settings = GetMutableDefault<UGP_ResourceGameplaySettings>())
		{
			SavedDropOffRetrySeconds = Settings->DropOffRetrySeconds;
			Settings->DropOffRetrySeconds = 0.25f;
			bSettingsOverridden = true;
		}
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("Case5SpawnScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Worker->GetCargoComponent()->ClearCargo();
		Cmd->DebugForceNextHaulApproachRejectOnce();
		IssueMine(Worker, Node);
		FillCargoFull(Worker);
		CargoAtWait = Worker->GetCargoComponent()->GetCurrentCargoAmount();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case5WaitingUnreachable"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), CargoAtWait), TEXT("Case5CargoPreserved"));
		Expect(!Cmd->IsHaulActive(), TEXT("Case5NotTravelActive"));
		StableWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 9:
	{
		// Stable for a few ticks (no rapid re-enter / travel).
		AGP_Worker* Worker = WorkerWeak.Get();
		if (!Expect(IsValid(Worker), TEXT("Case5StableWorker")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff
				|| Cmd->IsHaulActive()
				|| FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f),
			TEXT("Case5StableOrResumed"));
		if (Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff)
		{
			++StableWaitTicks;
			if (StableWaitTicks < 3)
			{
				Expect(!Cmd->IsHaulActive(), TEXT("Case5NoRapidTravel"));
				ScheduleNext();
				return;
			}
		}
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 10:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("Case5ResumeObjects"));
			Finish();
			return;
		}
		if (Worker->GetUnitCommandComponent()->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff)
		{
			if (MovementWaitTicks == 0)
			{
				MovementWaitStartTime = World->GetTimeSeconds();
			}
			++MovementWaitTicks;
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("Case5RetryResumeTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (WaitHaulOrMove(Worker, TEXT("Case5ResumeTimeout")))
		{
			return;
		}
		if (!Expect(IsValid(Worker) && IsValid(Base), TEXT("Case5ResumeObjects")))
		{
			Finish();
			return;
		}
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("Case5ResumedDeliver"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 11:
	{
		// Case 6 — Move while WaitingForDropOff cancels wait; later MainBase must not resurrect haul.
		CleanupActors();
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("Case6SpawnScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		ScenarioNodeLocation = Scenario.ResourceNodeLocation;
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
			MainBaseWeak.Reset();
		}
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		FillCargoFull(Worker);
		CargoAtWait = Worker->GetCargoComponent()->GetCurrentCargoAmount();
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case6EnteredWait"));
		const FVector MoveDest = Worker->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f);
		IssueMove(Worker, MoveDest);
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::Idle, TEXT("Case6HaulCleared"));
		Expect(Cmd->GetActiveHaulSerial() == 0, TEXT("Case6HaulSerialCleared"));
		Expect(!Cmd->DebugIsWaitingRegisterBound(), TEXT("Case6RegisterCleared"));
		Expect(!Cmd->DebugIsDropOffRetryArmed(), TEXT("Case6RetryCleared"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), CargoAtWait), TEXT("Case6CargoPreserved"));
		AGP_MainBase* Replacement = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ScenarioBaseLocation, ContractTeamId, OwnerTag);
		MainBaseWeak = Replacement;
		Expect(IsValid(Replacement), TEXT("Case6SpawnBaseAfterMove"));
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::Idle, TEXT("Case6NoResurrect"));
		Expect(!Cmd->IsHaulActive(), TEXT("Case6NoHaulResume"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), CargoAtWait), TEXT("Case6CargoStillHeld"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 12:
	{
		// Case 7 — partial cargo depletion + missing base → wait → deliver → PostDropOff path.
		CleanupActors();
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("Case7SpawnScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
			MainBaseWeak.Reset();
		}
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		const int32 Leave = 5;
		const int32 Consume = Node->GetCurrentAmount() - Leave;
		if (Consume > 0)
		{
			Node->ConsumeResource(Consume);
		}
		IssueMine(Worker, Node);
		Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		CargoAtWait = Worker->GetCargoComponent()->GetCurrentCargoAmount();
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(FMath::IsNearlyEqual(CargoAtWait, 5.0f, 0.1f), TEXT("Case7PartialCargo5"));
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case7Waiting"));
		Expect(!Cmd->ShouldReturnToDepositAfterHaul(), TEXT("Case7NoReturnToDeposit"));
		Expect(Cmd->HasHeldCommand(), TEXT("Case7HeldPersists"));
		ThreatBefore = 0.0f;
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			ThreatBefore = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
		}
		AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ScenarioBaseLocation, ContractTeamId, OwnerTag);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("Case7SpawnBase")))
		{
			Finish();
			return;
		}
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 13:
	{
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!IsValid(Worker) || !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("Case7Objects"));
			Finish();
			return;
		}
		if (Worker->GetUnitCommandComponent()->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff)
		{
			if (MovementWaitTicks == 0)
			{
				MovementWaitStartTime = World->GetTimeSeconds();
			}
			++MovementWaitTicks;
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("Case7WakeResumeTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (WaitHaulOrMove(Worker, TEXT("Case7HaulTimeout")))
		{
			return;
		}
		if (!Expect(IsValid(Worker) && IsValid(Base), TEXT("Case7Objects")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("Case7Unloaded"));
		Expect(Cmd->HasHeldCommand()
				|| Cmd->GetMineExecutionState() == EGP_MineExecutionState::WaitingForResource
				|| Cmd->GetMineTarget() != nullptr
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::WaitingForResource
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Mining,
			TEXT("Case7PostDropOffContinues"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
			Expect(ThreatAfter > ThreatBefore + KINDA_SMALL_NUMBER, TEXT("Case8ThreatOnlyAfterAccept"));
			Expect(FMath::IsNearlyEqual(
					Cmd->GetLastHaulThreatDelta(),
					Cmd->GetLastHaulAcceptedAmount() * Base->GetStorageComponent()->GetThreatPerStoredUnit(),
					0.05f),
				TEXT("Case8ThreatDeltaExact"));
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 14:
	{
		// Case 9 — subscription / timer uniqueness while waiting (fresh wait).
		CleanupActors();
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("Case9SpawnScenario")))
		{
			Finish();
			return;
		}
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;
		MainBaseWeak = Scenario.MainBase;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Scenario.MainBaseLocation;
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
			MainBaseWeak.Reset();
		}
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		FillCargoFull(Worker);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::WaitingForDropOff, TEXT("Case9Waiting"));
		Expect(Cmd->DebugIsWaitingRegisterBound(), TEXT("Case9SingleRegisterBound"));
		Expect(!Cmd->DebugIsActiveHaulUnregisterBound(), TEXT("Case9NoDualUnregister"));
		Expect(Cmd->DebugIsDropOffRetryArmed(), TEXT("Case9SingleRetryArmed"));
		Expect(Cmd->DebugGetDropOffWakeCount() == 0, TEXT("Case9NoWakeYet"));
		AGP_MainBase* Base = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ScenarioBaseLocation, ContractTeamId, OwnerTag);
		MainBaseWeak = Base;
		Expect(IsValid(Base), TEXT("Case9WakeBase"));
		Expect(Cmd->DebugGetDropOffWakeCount() == 1, TEXT("Case9WakeCountOne"));
		Expect(!Cmd->DebugIsWaitingRegisterBound() || Cmd->IsHaulActive()
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::DroppingOff
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToBase
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::Idle,
			TEXT("Case9RegisterClearedOrHauling"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 15:
	{
		// Case 10 — local regression markers for P2 invariants used by P3 continuation.
		AGP_Worker* Worker = WorkerWeak.Get();
		if (WaitHaulOrMove(Worker, TEXT("Case10HaulTimeout")))
		{
			return;
		}
		if (!Expect(IsValid(Worker), TEXT("Case10Worker")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::Idle
				|| Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit
				|| Cmd->GetMineExecutionState() != EGP_MineExecutionState::Idle
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::WaitingForResource
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Mining
				|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine,
			TEXT("Case10PostDropOffP2Path"));
		Expect(Cmd->DebugGetDropOffWakeCount() <= 1, TEXT("Case10NoDuplicateWake"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else
void UGP_DropOffResilienceContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_DropOffResilienceContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_DropOffResilienceContractTestRunner::ScheduleNext() {}
void UGP_DropOffResilienceContractTestRunner::AdvanceStage() {}
bool UGP_DropOffResilienceContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_DropOffResilienceContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_DropOffResilienceContractTestRunner::Finish() { bFinished = true; }
void UGP_DropOffResilienceContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_DropOffResilienceContractTestRunner::UnbindWorldCleanup() {}
void UGP_DropOffResilienceContractTestRunner::CleanupActors() {}
#endif // !UE_BUILD_SHIPPING
