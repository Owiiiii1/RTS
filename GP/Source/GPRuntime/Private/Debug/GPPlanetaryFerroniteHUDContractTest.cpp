// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPStorageComponent.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPlanetaryFerroniteHUD, Log, All);

namespace GPPlanetaryFerroniteHUDDebug
{
	static TWeakObjectPtr<UGP_PlanetaryFerroniteHUDContractTestRunner> GActiveRunner;

	static void RunPlanetaryFerroniteHUDContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPPlanetaryFerroniteHUD, Warning,
				TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("PlanetaryFerroniteHUDContract"), TEXT("PlanetaryFerroniteHUD"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_PlanetaryFerroniteHUDContractTestRunner* Runner =
			NewObject<UGP_PlanetaryFerroniteHUDContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GPlanetaryFerroniteHUDContract(
		TEXT("gp.Resource.RunPlanetaryFerroniteHUDContractTest"),
		TEXT("Authority: GP-S28P4 client-safe MainBase resolve + Storage HUD data-source contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunPlanetaryFerroniteHUDContractTest));
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::HandleStorageChanged(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)TotalCapacity;
	++StorageEventCount;
	LastStorageNewTotal = NewTotalStored;
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::HandleResolvedMainBaseChanged(
	int32 TeamId,
	AGP_MainBase* PreviousMainBase,
	AGP_MainBase* NewMainBase)
{
	(void)PreviousMainBase;
	++ResolvedChangeCount;
	LastResolvedTeamId = TeamId;
	LastResolvedNew = NewMainBase;
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::OnWorldCleanup(
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

void UGP_PlanetaryFerroniteHUDContractTestRunner::CleanupActors()
{
	auto DestroyWeak = [](auto& Weak)
	{
		if (Weak.IsValid())
		{
			Weak->Destroy();
			Weak.Reset();
		}
	};
	DestroyWeak(Team1BaseWeak);
	DestroyWeak(Team2BaseWeak);
	DestroyWeak(ReplacementWeak);
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->OnResolvedMainBaseChanged.RemoveAll(this);
		}
	}
	UnbindWorldCleanup();
	CleanupActors();

	UE_LOG(LogGPPlanetaryFerroniteHUD, Log,
		TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	GPPlanetaryFerroniteHUDDebug::GActiveRunner.Reset();
	RemoveFromRoot();
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPPlanetaryFerroniteHUD, Error,
		TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_PlanetaryFerroniteHUDContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPPlanetaryFerroniteHUD, Error,
			TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPPlanetaryFerroniteHUD, Log,
			TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_PlanetaryFerroniteHUDContractTestRunner::AdvanceStage));
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	ResolvedChangeCount = 0;
	StorageEventCount = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_PlanetaryFerroniteHUDContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPPlanetaryFerroniteHUD, Log, TEXT("GP Resource.RunPlanetaryFerroniteHUDContractTest Start"));
	ScheduleNext();
}

void UGP_PlanetaryFerroniteHUDContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	if (!Expect(IsValid(GS), TEXT("GameStatePresent")))
	{
		Finish();
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		GS->OnResolvedMainBaseChanged.AddUObject(
			this, &UGP_PlanetaryFerroniteHUDContractTestRunner::HandleResolvedMainBaseChanged);

		const int32 Team1 = GPResourceLoopDiagnostics::FindFreePlayableTeamId(World);
		if (!Expect(Team1 >= 1, TEXT("FreeTeam1")))
		{
			Finish();
			return;
		}

		FVector Loc(300.0f, 300.0f, 100.0f);
		GPResourceLoopDiagnostics::IsNavPointProjected(World, Loc, &Loc, 800.0f, 800.0f);
		AGP_MainBase* Base1 = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(World, Loc, Team1, OwnerTag);
		Team1BaseWeak = Base1;
		if (!Expect(IsValid(Base1), TEXT("SpawnTeam1Base")))
		{
			Finish();
			return;
		}

		Expect(GS->FindMainBaseForTeamClientSafe(Team1) == Base1, TEXT("A_ClientSafeFindsTeam1"));
		Expect(GS->FindMainBaseForTeam(Team1) == Base1, TEXT("A_AuthorityRegistryMatches"));
		Expect(ResolvedChangeCount >= 1 && LastResolvedTeamId == Team1 && LastResolvedNew.Get() == Base1,
			TEXT("A_ResolvedEventFired"));

		const int32 Team2 = GPResourceLoopDiagnostics::FindFreePlayableTeamId(World);
		Expect(Team2 >= 1 && Team2 != Team1, TEXT("FreeTeam2"));
		FVector Loc2 = Loc + FVector(600.0f, 0.0f, 0.0f);
		GPResourceLoopDiagnostics::IsNavPointProjected(World, Loc2, &Loc2, 800.0f, 800.0f);
		AGP_MainBase* Base2 = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(World, Loc2, Team2, OwnerTag);
		Team2BaseWeak = Base2;
		Expect(IsValid(Base2), TEXT("SpawnTeam2Base"));
		Expect(GS->FindMainBaseForTeamClientSafe(Team1) == Base1, TEXT("C_Team1Unchanged"));
		Expect(GS->FindMainBaseForTeamClientSafe(Team2) == Base2, TEXT("C_Team2Lookup"));
		Expect(GS->FindMainBaseForTeamClientSafe(Team1) != Base2, TEXT("C_Team2NotReturnedForTeam1"));

		UGP_StorageComponent* Storage = Base1->GetStorageComponent();
		Expect(IsValid(Storage), TEXT("F_StoragePresent"));
		const float InitialStored = Storage->GetTotalStored();
		Expect(FMath::IsNearlyEqual(InitialStored, 0.0f), TEXT("F_InitialStoredZero"));
		Storage->OnStorageChanged.AddDynamic(this, &UGP_PlanetaryFerroniteHUDContractTestRunner::HandleStorageChanged);
		const FGP_StorageAddResult AddResult = Storage->AddPlanetaryFerronite(50.0f);
		Expect(AddResult.Accepted >= 49.0f, TEXT("G_Accepted50"));
		Expect(StorageEventCount >= 1, TEXT("G_StorageEventFired"));
		Expect(FMath::IsNearlyEqual(LastStorageNewTotal, Storage->GetTotalStored(), 0.05f), TEXT("G_EventMatchesTotal"));
		Expect(FMath::IsNearlyEqual(Storage->GetTotalStored(), InitialStored + AddResult.Accepted, 0.05f),
			TEXT("G_TotalUpdated"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_MainBase* Base1 = Team1BaseWeak.Get();
		if (!Expect(IsValid(Base1), TEXT("D_BaseBeforeDestroy")))
		{
			Finish();
			return;
		}
		const int32 Team1 = Base1->GetTeamId();
		ResolvedChangeCount = 0;
		Base1->Destroy();
		Team1BaseWeak.Reset();
		Expect(GS->FindMainBaseForTeamClientSafe(Team1) == nullptr, TEXT("D_UnregisterClearsResolve"));
		Expect(ResolvedChangeCount >= 1 && LastResolvedNew.Get() == nullptr, TEXT("D_ResolvedClearedEvent"));

		FVector Loc(320.0f, 320.0f, 100.0f);
		GPResourceLoopDiagnostics::IsNavPointProjected(World, Loc, &Loc, 800.0f, 800.0f);
		AGP_MainBase* Replacement = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(World, Loc, Team1, OwnerTag);
		ReplacementWeak = Replacement;
		Expect(IsValid(Replacement), TEXT("E_SpawnReplacement"));
		Expect(GS->FindMainBaseForTeamClientSafe(Team1) == Replacement, TEXT("E_ReplacementResolve"));
		Expect(LastResolvedNew.Get() == Replacement, TEXT("E_ReplacementEvent"));

		Expect(true, TEXT("H_NoTickRequired"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else
void UGP_PlanetaryFerroniteHUDContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_PlanetaryFerroniteHUDContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_PlanetaryFerroniteHUDContractTestRunner::ScheduleNext() {}
void UGP_PlanetaryFerroniteHUDContractTestRunner::AdvanceStage() {}
bool UGP_PlanetaryFerroniteHUDContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_PlanetaryFerroniteHUDContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_PlanetaryFerroniteHUDContractTestRunner::Finish() { bFinished = true; }
void UGP_PlanetaryFerroniteHUDContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_PlanetaryFerroniteHUDContractTestRunner::UnbindWorldCleanup() {}
void UGP_PlanetaryFerroniteHUDContractTestRunner::CleanupActors() {}
void UGP_PlanetaryFerroniteHUDContractTestRunner::HandleStorageChanged(float PreviousTotalStored, float NewTotalStored, float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)NewTotalStored;
	(void)TotalCapacity;
}
void UGP_PlanetaryFerroniteHUDContractTestRunner::HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* PreviousMainBase, AGP_MainBase* NewMainBase)
{
	(void)TeamId;
	(void)PreviousMainBase;
	(void)NewMainBase;
}
#endif // !UE_BUILD_SHIPPING
