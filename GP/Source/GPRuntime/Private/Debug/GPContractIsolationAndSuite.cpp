// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPS28Suite, Log, All);

namespace GPS28SuiteDebug
{
	TWeakObjectPtr<UGP_S28RegressionSuiteRunner> GActiveSuite;
	TWeakObjectPtr<UGP_ContractIsolationContractTestRunner> GActiveIsolation;

	static void RunS28RegressionSuite(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPS28Suite, Warning, TEXT("GP Resource.RunS28RegressionSuite: missing world or client"));
			return;
		}
		if (GActiveSuite.IsValid())
		{
			UE_LOG(LogGPS28Suite, Warning, TEXT("GP Resource.RunS28RegressionSuite: rejected — suite already running"));
			return;
		}
		if (GPContractTestCoordinator::IsBusy())
		{
			UE_LOG(LogGPS28Suite, Warning,
				TEXT("ContractTestRejected: Requested=S28RegressionSuite Active=%s Reason=AnotherContractTestRunning"),
				*GPContractTestCoordinator::GetActiveTestName().ToString());
			return;
		}

		UGP_S28RegressionSuiteRunner* Runner = NewObject<UGP_S28RegressionSuiteRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveSuite = Runner;
		Runner->Start(World);
	}

	static void RunContractIsolationTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPS28Suite, Warning, TEXT("GP Resource.RunContractIsolationContractTest: missing world or client"));
			return;
		}
		if (GActiveIsolation.IsValid())
		{
			UE_LOG(LogGPS28Suite, Warning, TEXT("GP Resource.RunContractIsolationContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("ContractIsolation"), TEXT("ContractIsolation"), Token))
		{
			return;
		}

		UGP_ContractIsolationContractTestRunner* Runner =
			NewObject<UGP_ContractIsolationContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveIsolation = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GS28Suite(
		TEXT("gp.Resource.RunS28RegressionSuite"),
		TEXT("Run GP-S28 contract tests strictly sequentially (waits for each token release)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunS28RegressionSuite));

	static FAutoConsoleCommandWithWorldAndArgs GIsolation(
		TEXT("gp.Resource.RunContractIsolationContractTest"),
		TEXT("Coordinator mutual exclusion, owned cleanup isolation, async actor-loss null-safety."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunContractIsolationTest));
}

void UGP_S28RegressionSuiteRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_S28RegressionSuiteRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_S28RegressionSuiteRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Finish();
	}
}

void UGP_S28RegressionSuiteRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	SuiteIndex = 0;
	Failures = 0;
	WaitingForExecutionId = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_S28RegressionSuiteRunner::OnWorldCleanup);

	UE_LOG(LogGPS28Suite, Log, TEXT("GP-S28 RegressionSuite Start"));
	GPContractTestCoordinator::SetFinishCallback(
		GPContractTestCoordinator::FOnContractFinished::CreateUObject(
			this, &UGP_S28RegressionSuiteRunner::OnChildFinished));
	StartNext();
}

void UGP_S28RegressionSuiteRunner::StartNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		Finish();
		return;
	}

	static const TCHAR* Commands[] = {
		TEXT("gp.Cargo.RunContractTest"),
		TEXT("gp.Resource.RunPresentationContractTest"),
		TEXT("gp.Mining.RunContractTest"),
		TEXT("gp.Worker.RunContractTest"),
		TEXT("gp.Worker.RunHaulingContractTest"),
		TEXT("gp.Storage.RunContractTest"),
		TEXT("gp.Resource.RunDiagnosticScenarioContractTest"),
		TEXT("gp.Resource.RunEndPlayContractTest"),
		TEXT("gp.Resource.RunDepletionReassignmentContractTest"),
		TEXT("gp.Resource.RunDropOffResilienceContractTest"),
	};
	constexpr int32 NumCommands = UE_ARRAY_COUNT(Commands);

	if (SuiteIndex >= NumCommands)
	{
		Finish();
		return;
	}

	if (GPContractTestCoordinator::IsBusy())
	{
		UE_LOG(LogGPS28Suite, Warning,
			TEXT("GP-S28 RegressionSuite deferring %s — Active=%s"),
			Commands[SuiteIndex],
			*GPContractTestCoordinator::GetActiveTestName().ToString());
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UGP_S28RegressionSuiteRunner::StartNext));
		return;
	}

	const int32 LaunchIndex = SuiteIndex;
	const TCHAR* Command = Commands[LaunchIndex];
	UE_LOG(LogGPS28Suite, Log, TEXT("GP-S28 RegressionSuite Launching[%d]=%s"), LaunchIndex, Command);
	if (GEngine)
	{
		GEngine->Exec(World, Command);
	}

	WaitingForExecutionId = GPContractTestCoordinator::GetActiveExecutionId();

	// Sync child (Cargo) completes inside Exec via finish callback (SuiteIndex already advanced).
	// If nothing started / callback missed, fail-forward next tick.
	if (!GPContractTestCoordinator::IsBusy() && SuiteIndex == LaunchIndex)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[this, LaunchIndex]()
			{
				if (!bFinished && SuiteIndex == LaunchIndex)
				{
					++Failures;
					UE_LOG(LogGPS28Suite, Error,
						TEXT("GP-S28 RegressionSuite FAIL: ChildDidNotStart index=%d"), LaunchIndex);
					++SuiteIndex;
					StartNext();
				}
			}));
	}
}

void UGP_S28RegressionSuiteRunner::OnChildFinished(uint64 ExecutionId, int32 ChildFailures)
{
	(void)ExecutionId;
	if (bFinished)
	{
		return;
	}

	Failures += ChildFailures;
	UE_LOG(LogGPS28Suite, Log,
		TEXT("GP-S28 RegressionSuite ChildComplete Index=%d Failures+=%d TotalFailures=%d"),
		SuiteIndex,
		ChildFailures,
		Failures);

	if (ChildFailures > 0)
	{
		UE_LOG(LogGPS28Suite, Error,
			TEXT("GP-S28 RegressionSuite stopping after failure at index=%d"), SuiteIndex);
		Finish();
		return;
	}

	++SuiteIndex;
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UGP_S28RegressionSuiteRunner::StartNext));
	}
	else
	{
		Finish();
	}
}

void UGP_S28RegressionSuiteRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	UnbindWorldCleanup();
	GPContractTestCoordinator::ClearFinishCallback();
	UE_LOG(LogGPS28Suite, Log, TEXT("GP-S28 RegressionSuite Complete Failures=%d"), Failures);
	RemoveFromRoot();
	GPS28SuiteDebug::GActiveSuite.Reset();
	WorldWeak.Reset();
}

void UGP_ContractIsolationContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_ContractIsolationContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_ContractIsolationContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Finish();
	}
}

void UGP_ContractIsolationContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;

	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_ContractIsolationContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPS28Suite, Log,
		TEXT("GP Resource.RunContractIsolationContractTest Stage=Start ExecutionId=%llu OwnerTag=%s"),
		ExecutionId,
		*OwnerTag.ToString());
	ScheduleNext();
}

void UGP_ContractIsolationContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished || !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		if (!bFinished && ExecutionId != 0 && !GPContractTestCoordinator::IsTokenActive(ExecutionId))
		{
			return;
		}
		if (!bFinished && !IsValid(World))
		{
			Abort(TEXT("WorldInvalid"));
		}
		return;
	}
	if (GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		Abort(TEXT("WorldEndPlay"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_ContractIsolationContractTestRunner::AdvanceStage));
}

bool UGP_ContractIsolationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPS28Suite, Error, TEXT("GP Resource.RunContractIsolationContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPS28Suite, Log, TEXT("GP Resource.RunContractIsolationContractTest PASS: %s"), Label);
	return true;
}

void UGP_ContractIsolationContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPS28Suite, Error, TEXT("GP Resource.RunContractIsolationContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_ContractIsolationContractTestRunner::Finish()
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

	GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(WorldWeak.Get(), OwnerTag);
	if (AGP_MainBase* OpBase = OperatorBaseWeak.Get())
	{
		if (IsValid(OpBase))
		{
			OpBase->Destroy();
		}
	}
	if (AGP_Worker* OpWorker = OperatorWorkerWeak.Get())
	{
		if (IsValid(OpWorker))
		{
			OpWorker->Destroy();
		}
	}
	if (AGP_ResourceNode* OpNode = OperatorNodeWeak.Get())
	{
		if (IsValid(OpNode))
		{
			OpNode->Destroy();
		}
	}
	OperatorBaseWeak.Reset();
	OperatorWorkerWeak.Reset();
	OperatorNodeWeak.Reset();
	HaulWorkerWeak.Reset();

	UE_LOG(LogGPS28Suite, Log,
		TEXT("GP Resource.RunContractIsolationContractTest: Complete Failures=%d"), Failures);
	if (ExecutionId != 0)
	{
		GPContractTestCoordinator::Release(ExecutionId, Failures, Failures > 0, Failures > 0 ? TEXT("Failed") : TEXT("None"));
		ExecutionId = 0;
	}
	RemoveFromRoot();
	GPS28SuiteDebug::GActiveIsolation.Reset();
	WorldWeak.Reset();
}

void UGP_ContractIsolationContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (bFinished)
	{
		return;
	}
	if (!IsValid(World) || GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		Abort(TEXT("WorldEndPlay"));
		return;
	}
	if (ExecutionId != 0 && !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		UE_LOG(LogGPS28Suite, Log, TEXT("GP Resource.RunContractIsolationContractTest Stage=ContractRunnerMutualExclusion"));
		GPContractTestCoordinator::FExecutionToken RejectedToken;
		const bool bRejected = !GPContractTestCoordinator::TryAcquire(
			World, TEXT("IsolationProbeB"), TEXT("IsolationProbeB"), RejectedToken);
		Expect(bRejected, TEXT("RunnerBRejectedWhileAHoldsToken"));
		Expect(GPContractTestCoordinator::IsTokenActive(ExecutionId), TEXT("RunnerAStillHoldsToken"));
		Expect(RejectedToken.ExecutionId == 0, TEXT("RejectedTokenEmpty"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		UE_LOG(LogGPS28Suite, Log, TEXT("GP Resource.RunContractIsolationContractTest Stage=ContractOwnedCleanupIsolation"));
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors OperatorScenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(
				World, 1, GPContractTestCoordinator::OwnerTagOperator);
		if (!Expect(OperatorScenario.bOk, TEXT("OperatorScenarioSpawnOk")))
		{
			Finish();
			return;
		}
		OperatorBaseWeak = OperatorScenario.MainBase;
		OperatorWorkerWeak = OperatorScenario.Worker;
		OperatorNodeWeak = OperatorScenario.ResourceNode;
		Expect(OperatorScenario.TeamId == 1, TEXT("OperatorOnTeam1"));

		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors ContractScenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (ContractScenario.Error == TEXT("BlockedByOccupiedPlayableTeams"))
		{
			Expect(false, TEXT("BlockedByOccupiedPlayableTeams"));
			Finish();
			return;
		}
		if (!Expect(ContractScenario.bOk, TEXT("ContractScenarioSpawnOk")))
		{
			Finish();
			return;
		}
		ContractTeamId = ContractScenario.TeamId;
		Expect(ContractTeamId != 1, TEXT("ContractRemappedOffOperatorTeam1"));
		Expect(GPResourceLoopDiagnostics::ActorHasOwnerTag(ContractScenario.MainBase, OwnerTag),
			TEXT("ContractExactOwnerTag"));

		AGP_MainBase* OpBaseBefore = OperatorBaseWeak.Get();
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
		Expect(IsValid(OpBaseBefore) && IsValid(OperatorBaseWeak.Get()), TEXT("OperatorBaseSurvivedContractCleanup"));
		Expect(IsValid(OperatorWorkerWeak.Get()), TEXT("OperatorWorkerSurvivedContractCleanup"));
		Expect(IsValid(OperatorNodeWeak.Get()), TEXT("OperatorNodeSurvivedContractCleanup"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			Expect(GS->FindMainBaseForTeam(1) == OpBaseBefore, TEXT("OperatorTeam1RegistryUnchanged"));
		}
		Expect(!IsValid(ContractScenario.MainBase), TEXT("ContractActorsCleanedByOwnerTag"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		UE_LOG(LogGPS28Suite, Log, TEXT("GP Resource.RunContractIsolationContractTest Stage=AsyncActorLossNullSafety"));
		// Local owned worker under our OwnerTag; destroy it; next stage must fail controlled (no AV).
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && IsValid(Scenario.Worker), TEXT("AsyncLossScenarioSpawn")))
		{
			Finish();
			return;
		}
		HaulWorkerWeak = Scenario.Worker;
		ContractTeamId = Scenario.TeamId;
		HaulWorkerWeak.Get()->Destroy();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		AGP_Worker* Worker = HaulWorkerWeak.Get();
		if (!IsValid(Worker))
		{
			Expect(true, TEXT("AsyncActorLossNullSafety"));
			Expect(true, TEXT("CancelReasonOwnedActorDestroyedPath"));
		}
		else
		{
			Expect(false, TEXT("AsyncActorLossWorkerShouldBeInvalid"));
		}
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else

void UGP_S28RegressionSuiteRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_S28RegressionSuiteRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_S28RegressionSuiteRunner::StartNext() {}
void UGP_S28RegressionSuiteRunner::OnChildFinished(uint64 ExecutionId, int32 ChildFailures)
{
	(void)ExecutionId;
	(void)ChildFailures;
}
void UGP_S28RegressionSuiteRunner::Finish() { bFinished = true; }
void UGP_S28RegressionSuiteRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_S28RegressionSuiteRunner::UnbindWorldCleanup() {}

void UGP_ContractIsolationContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_ContractIsolationContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_ContractIsolationContractTestRunner::ScheduleNext() {}
void UGP_ContractIsolationContractTestRunner::AdvanceStage() {}
bool UGP_ContractIsolationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_ContractIsolationContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_ContractIsolationContractTestRunner::Finish() { bFinished = true; }
void UGP_ContractIsolationContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_ContractIsolationContractTestRunner::UnbindWorldCleanup() {}

#endif // !UE_BUILD_SHIPPING
