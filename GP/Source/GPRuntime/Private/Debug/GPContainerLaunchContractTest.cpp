// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPStorageComponent.h"

#if !UE_BUILD_SHIPPING

#include "AttributeSets/GPPlayerAttributeSet.h"
#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"

namespace GPContainerLaunchDebug
{
	static TWeakObjectPtr<UGP_ContainerLaunchContractTestRunner> GActiveRunner;

	static AGP_PlayerState* SpawnTeamPlayerState(UWorld* World, AGameStateBase* GameState, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerState* PS = World->SpawnActor<AGP_PlayerState>(
			AGP_PlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(PS) || GameState == nullptr)
		{
			return nullptr;
		}
		PS->SetTeamId(TeamId);
		GameState->AddPlayerState(PS);
		if (UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, PS);
		}
		return PS;
	}

	static void ForceFillReady(UGP_StorageComponent* Storage, float Amount)
	{
		if (!IsValid(Storage))
		{
			return;
		}
		Storage->AddPlanetaryFerronite(Amount);
	}

	static void RunContainerLaunchContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Resource.RunContainerLaunchContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Resource.RunContainerLaunchContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("ContainerLaunchContract"), TEXT("ContainerLaunch"), Token))
		{
			return;
		}

		UGP_ContainerLaunchContractTestRunner* Runner =
			NewObject<UGP_ContainerLaunchContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static void LaunchReadyContainerCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Resource.LaunchReadyContainer: missing world or client"));
			return;
		}

		AGP_MainBase* Chosen = nullptr;
		if (Args.Num() >= 1)
		{
			for (TActorIterator<AGP_MainBase> It(World); It; ++It)
			{
				if (It->GetName().Contains(Args[0]))
				{
					Chosen = *It;
					break;
				}
			}
		}
		if (!IsValid(Chosen))
		{
			if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
			{
				Chosen = GS->FindMainBaseForTeam(1);
			}
		}
		if (!IsValid(Chosen) || Chosen->GetStorageComponent() == nullptr)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Resource.LaunchReadyContainer: no MainBase/Storage"));
			return;
		}

		const FGP_ContainerLaunchResult Result = Chosen->GetStorageComponent()->TryLaunchReadyContainer();
		UE_LOG(LogGPStorage, Log,
			TEXT("GP Resource.LaunchReadyContainer: Base=%s Accepted=%s Reason=%d Index=%d Amount=%.3f Duration=%.3f"),
			*Chosen->GetName(),
			Result.bAccepted ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Result.RejectReason),
			Result.ContainerIndex,
			Result.LaunchedPlanetaryAmount,
			Result.LaunchDurationSeconds);
	}

	static FAutoConsoleCommandWithWorldAndArgs GContainerLaunchContract(
		TEXT("gp.Resource.RunContainerLaunchContractTest"),
		TEXT("Authority: GP-S30 container launch / orbital conversion contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunContainerLaunchContractTest));

	static FAutoConsoleCommandWithWorldAndArgs GLaunchReadyContainer(
		TEXT("gp.Resource.LaunchReadyContainer"),
		TEXT("Authority: production TryLaunchReadyContainer on MainBase. Usage: gp.Resource.LaunchReadyContainer [NameSubstring]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LaunchReadyContainerCommand));
}

void UGP_ContainerLaunchContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_ContainerLaunchContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_ContainerLaunchContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_ContainerLaunchContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (AGP_PlayerState* OwnerPS = OwnerPSWeak.Get())
			{
				GS->RemovePlayerState(OwnerPS);
			}
			if (AGP_PlayerState* OtherPS = OtherPSWeak.Get())
			{
				GS->RemovePlayerState(OtherPS);
			}
		}
	}
	auto DestroyWeak = [](auto& Weak)
	{
		if (Weak.IsValid())
		{
			Weak->Destroy();
			Weak.Reset();
		}
	};
	DestroyWeak(MainBaseWeak);
	DestroyWeak(OwnerPSWeak);
	DestroyWeak(OtherPSWeak);
}

void UGP_ContainerLaunchContractTestRunner::Finish()
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

	UE_LOG(LogGPStorage, Log,
		TEXT("GP Resource.RunContainerLaunchContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	if (GPContainerLaunchDebug::GActiveRunner.Get() == this)
	{
		GPContainerLaunchDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_ContainerLaunchContractTestRunner::Abort(const TCHAR* Reason)
{
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPStorage, Error, TEXT("GP Resource.RunContainerLaunchContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_ContainerLaunchContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPStorage, Error, TEXT("GP Resource.RunContainerLaunchContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPStorage, Log, TEXT("GP Resource.RunContainerLaunchContractTest PASS: %s"), Label);
	return true;
}

void UGP_ContainerLaunchContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_ContainerLaunchContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_ContainerLaunchContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_ContainerLaunchContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPStorage, Log, TEXT("GP Resource.RunContainerLaunchContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_ContainerLaunchContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
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
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-46000.0f, 1000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base) && Base->GetStorageComponent() != nullptr, TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(7);

		AGP_PlayerState* OwnerPS = GPContainerLaunchDebug::SpawnTeamPlayerState(World, GS, 7);
		AGP_PlayerState* OtherPS = GPContainerLaunchDebug::SpawnTeamPlayerState(World, GS, 8);
		OwnerPSWeak = OwnerPS;
		OtherPSWeak = OtherPS;
		if (!Expect(IsValid(OwnerPS) && IsValid(OtherPS), TEXT("SpawnPlayerStates")))
		{
			Finish();
			return;
		}

		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		FGP_ContainerLaunchResult EmptyLaunch = Storage->TryLaunchReadyContainer();
		Expect(!EmptyLaunch.bAccepted, TEXT("A_EmptyReject"));
		Expect(EmptyLaunch.RejectReason == EGP_ContainerLaunchRejectReason::NoReadyContainer, TEXT("A_EmptyReason"));

		Storage->AddPlanetaryFerronite(40.0f);
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Filling, TEXT("A_FillingState"));
		FGP_ContainerLaunchResult FillingLaunch = Storage->TryLaunchReadyContainer();
		Expect(!FillingLaunch.bAccepted, TEXT("A_FillingReject"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 0.0f), TEXT("A_NoOrbitalReward"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetFerroniteScore(), 0.0f), TEXT("A_NoScoreReward"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_PlayerState* OtherPS = OtherPSWeak.Get();
		if (!Expect(IsValid(Base) && IsValid(OwnerPS) && IsValid(OtherPS), TEXT("B_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		Storage->AddPlanetaryFerronite(100.0f); // fill remaining 60 to Ready (40 already)
		Expect(Storage->GetReadyCount() >= 1, TEXT("C_ReadyPresent"));
		LaunchAmount = Storage->GetContainers()[0].CurrentAmount;
		ExpectedOrbital = LaunchAmount * Storage->GetOrbitalConversionRate();
		ExpectedScore = LaunchAmount * Storage->GetScoreConversionRate();
		ExpectedThreatDelta = LaunchAmount * Storage->GetThreatPerStoredUnit();
		OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		ScoreBefore = OwnerPS->GetPlayerAttributeSet()->GetFerroniteScore();
		StoredBefore = Storage->GetTotalStored();
		ThreatBefore = GS->GetFerroniteThreatValueForTeam(7);

		// Seed threat as if drop-off already applied.
		GS->AddFerroniteThreatValueForTeam(7, ExpectedThreatDelta);
		ThreatBefore = GS->GetFerroniteThreatValueForTeam(7);

		const FGP_ContainerLaunchResult Launch = Storage->TryLaunchReadyContainer();
		Expect(Launch.bAccepted, TEXT("C_LaunchAccepted"));
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("C_LaunchingState"));
		Expect(Storage->IsLaunchInFlight(), TEXT("C_LaunchInFlight"));

		const FGP_ContainerLaunchResult Second = Storage->TryLaunchReadyContainer();
		Expect(!Second.bAccepted, TEXT("D_SecondLaunchRejected"));
		Expect(Second.RejectReason == EGP_ContainerLaunchRejectReason::LaunchInFlight, TEXT("D_SecondReason"));

		const FGP_StorageAddResult AddDuring = Storage->AddPlanetaryFerronite(10.0f);
		Expect(AddDuring.Accepted <= KINDA_SMALL_NUMBER
			|| Storage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("D_NoFillIntoLaunchingSlot0"));
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("D_LaunchingPreserved"));
		Expect(FMath::IsNearlyEqual(Storage->GetContainers()[0].CurrentAmount, LaunchAmount, 0.05f), TEXT("D_AmountProtected"));
		// Capture after protected add so completion math accounts for fill into other slots.
		StoredBefore = Storage->GetTotalStored();

		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore), TEXT("D_NoRewardYetOrbital"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetFerroniteScore(), ScoreBefore), TEXT("D_NoRewardYetScore"));

		float Duration = Launch.LaunchDurationSeconds;
		if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
		{
			Duration = Settings->ContainerLaunchDurationSeconds;
		}
		++StageIndex;
		ScheduleNext(Duration + 0.15f);
		break;
	}
	case 2:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_PlayerState* OtherPS = OtherPSWeak.Get();
		if (!Expect(IsValid(Base) && IsValid(OwnerPS) && IsValid(OtherPS), TEXT("E_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		Expect(!Storage->IsLaunchInFlight(), TEXT("E_LaunchFinished"));
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Empty, TEXT("E_EmptyAfterLaunch"));
		Expect(FMath::IsNearlyEqual(Storage->GetContainers()[0].CurrentAmount, 0.0f), TEXT("E_AmountZero"));
		Expect(FMath::IsNearlyEqual(Storage->GetTotalStored(), StoredBefore - LaunchAmount, 0.05f), TEXT("E_TotalStoredDecreased"));

		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBefore + ExpectedOrbital,
			0.05f), TEXT("F_OrbitalIncreased"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetFerroniteScore(),
			ScoreBefore + ExpectedScore,
			0.05f), TEXT("F_ScoreIncreased"));
		Expect(FMath::IsNearlyEqual(OtherPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 0.0f), TEXT("H_OtherOrbitalUnchanged"));
		Expect(FMath::IsNearlyEqual(OtherPS->GetPlayerAttributeSet()->GetFerroniteScore(), 0.0f), TEXT("H_OtherScoreUnchanged"));

		const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(7);
		Expect(FMath::IsNearlyEqual(ThreatAfter, ThreatBefore - ExpectedThreatDelta, 0.05f), TEXT("G_ThreatDecreased"));
		Expect(ThreatAfter >= -KINDA_SMALL_NUMBER, TEXT("G_ThreatNonNegative"));

		// I — invalid owner: temporarily remove matching PlayerState
		Storage->AddPlanetaryFerronite(100.0f);
		Expect(Storage->GetReadyCount() >= 1, TEXT("I_ReadyAgain"));
		GS->RemovePlayerState(OwnerPS);
		OwnerPSWeak.Reset();
		OwnerPS->Destroy();
		const FGP_ContainerLaunchResult NoOwner = Storage->TryLaunchReadyContainer();
		Expect(!NoOwner.bAccepted, TEXT("I_MissingOwnerReject"));
		Expect(NoOwner.RejectReason == EGP_ContainerLaunchRejectReason::MissingPlayerState, TEXT("I_MissingOwnerReason"));
		Expect(Storage->GetReadyCount() >= 1, TEXT("I_ReadyPreserved"));

		// Restore owner and prove refill after empty works (J)
		AGP_PlayerState* Restored = GPContainerLaunchDebug::SpawnTeamPlayerState(World, GS, 7);
		OwnerPSWeak = Restored;
		Expect(IsValid(Restored), TEXT("J_RestoreOwner"));
		const float StoredMid = Storage->GetTotalStored();
		Storage->AddPlanetaryFerronite(25.0f);
		Expect(Storage->GetTotalStored() > StoredMid + 20.0f, TEXT("J_CanFillAgain"));

		Expect(true, TEXT("SuiteComplete"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_ContainerLaunchContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_ContainerLaunchContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_ContainerLaunchContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_ContainerLaunchContractTestRunner::AdvanceStage() {}
bool UGP_ContainerLaunchContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_ContainerLaunchContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_ContainerLaunchContractTestRunner::Finish() { bFinished = true; }
void UGP_ContainerLaunchContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_ContainerLaunchContractTestRunner::UnbindWorldCleanup() {}
void UGP_ContainerLaunchContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
