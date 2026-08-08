// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPStorageComponent.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "GameplayEffectTypes.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "TimerManager.h"
#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPContainerLaunchHUD, Log, All);

namespace GPContainerLaunchHUDDebug
{
	static TWeakObjectPtr<UGP_ContainerLaunchHUDContractTestRunner> GActiveRunner;

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

	static AGP_PlayerController* SpawnTeamPC(UWorld* World, AGP_PlayerState* PS)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerController* PC = World->SpawnActor<AGP_PlayerController>(
			AGP_PlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(PC) || !IsValid(PS))
		{
			return nullptr;
		}
		PC->PlayerState = PS;
		PS->SetOwner(PC);
		return PC;
	}

	static void RunContainerLaunchHUDContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPContainerLaunchHUD, Warning,
				TEXT("GP Resource.RunContainerLaunchHUDContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPContainerLaunchHUD, Warning,
				TEXT("GP Resource.RunContainerLaunchHUDContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("ContainerLaunchHUDContract"), TEXT("ContainerLaunchHUD"), Token))
		{
			return;
		}

		UGP_ContainerLaunchHUDContractTestRunner* Runner =
			NewObject<UGP_ContainerLaunchHUDContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GContainerLaunchHUDContract(
		TEXT("gp.Resource.RunContainerLaunchHUDContractTest"),
		TEXT("Authority: GP-S30 TEMP HUD Orbital + Launch button / PC server launch request contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunContainerLaunchHUDContractTest));
}

void UGP_ContainerLaunchHUDContractTestRunner::HandleOrbitalAttrChanged(const FOnAttributeChangeData& Data)
{
	++OrbitalAttrEventCount;
	LastOrbitalAttrValue = Data.NewValue;
	if (UGP_TEMP_S28P_PlanetaryFerroniteHUD* HUD = HUDWeak.Get())
	{
		HUD->SetOrbitalFerroniteDisplay(Data.NewValue);
	}
}

void UGP_ContainerLaunchHUDContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_ContainerLaunchHUDContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_ContainerLaunchHUDContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_ContainerLaunchHUDContractTestRunner::CleanupActors()
{
	if (UGP_AbilitySystemComponent* ASC = BoundASCWeak.Get())
	{
		if (OrbitalAttrHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).Remove(OrbitalAttrHandle);
		}
	}
	OrbitalAttrHandle.Reset();
	BoundASCWeak.Reset();

	if (UGP_TEMP_S28P_PlanetaryFerroniteHUD* HUD = HUDWeak.Get())
	{
		HUD->RemoveFromParent();
		HUDWeak.Reset();
	}

	auto DestroyWeak = [](auto& Weak)
	{
		if (Weak.IsValid())
		{
			Weak->Destroy();
			Weak.Reset();
		}
	};
	DestroyWeak(OwnPCWeak);
	DestroyWeak(OtherPCWeak);
	DestroyWeak(OwnBaseWeak);
	DestroyWeak(OtherBaseWeak);
	DestroyWeak(OwnPSWeak);
	DestroyWeak(OtherPSWeak);
}

void UGP_ContainerLaunchHUDContractTestRunner::Finish()
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

	UE_LOG(LogGPContainerLaunchHUD, Log,
		TEXT("GP Resource.RunContainerLaunchHUDContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	if (GPContainerLaunchHUDDebug::GActiveRunner.Get() == this)
	{
		GPContainerLaunchHUDDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_ContainerLaunchHUDContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPContainerLaunchHUD, Error,
		TEXT("GP Resource.RunContainerLaunchHUDContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_ContainerLaunchHUDContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPContainerLaunchHUD, Error,
			TEXT("GP Resource.RunContainerLaunchHUDContractTest FAIL: %s"), Label);
	}
	else
	{
		UE_LOG(LogGPContainerLaunchHUD, Log,
			TEXT("GP Resource.RunContainerLaunchHUDContractTest PASS: %s"), Label);
	}
	return bOk;
}

void UGP_ContainerLaunchHUDContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		Finish();
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_ContainerLaunchHUDContractTestRunner::AdvanceStage),
		FMath::Max(0.01f, DelaySeconds),
		false);
}

void UGP_ContainerLaunchHUDContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	OrbitalAttrEventCount = 0;
	LastOrbitalAttrValue = -1.0f;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_ContainerLaunchHUDContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPContainerLaunchHUD, Log, TEXT("GP Resource.RunContainerLaunchHUDContractTest Start"));
	ScheduleNext(0.05f);
}

void UGP_ContainerLaunchHUDContractTestRunner::AdvanceStage()
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
		AGP_PlayerState* OwnPS = GPContainerLaunchHUDDebug::SpawnTeamPlayerState(World, GS, 11);
		AGP_PlayerState* OtherPS = GPContainerLaunchHUDDebug::SpawnTeamPlayerState(World, GS, 12);
		OwnPSWeak = OwnPS;
		OtherPSWeak = OtherPS;
		if (!Expect(IsValid(OwnPS) && IsValid(OtherPS), TEXT("SpawnPlayerStates")))
		{
			Finish();
			return;
		}

		AGP_PlayerController* OwnPC = GPContainerLaunchHUDDebug::SpawnTeamPC(World, OwnPS);
		AGP_PlayerController* OtherPC = GPContainerLaunchHUDDebug::SpawnTeamPC(World, OtherPS);
		OwnPCWeak = OwnPC;
		OtherPCWeak = OtherPC;
		if (!Expect(IsValid(OwnPC) && IsValid(OtherPC), TEXT("SpawnPlayerControllers")))
		{
			Finish();
			return;
		}

		UGP_TEMP_S28P_PlanetaryFerroniteHUD* HUD =
			CreateWidget<UGP_TEMP_S28P_PlanetaryFerroniteHUD>(World, UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass());
		HUDWeak = HUD;
		if (!Expect(IsValid(HUD), TEXT("A_CreateHUD")))
		{
			Finish();
			return;
		}
		HUD->SetOwningPlayer(OwnPC);
		HUD->AddToViewport(1);
		HUD->SetPlanetaryFerroniteDisplay(0.0f, false);
		HUD->SetOrbitalFerroniteDisplay(0.0f);
		HUD->SetLaunchButtonEnabled(false);

		Expect(HUD->GetVisibility() == ESlateVisibility::SelfHitTestInvisible, TEXT("A_RootSelfHitTestInvisible"));
		Expect(HUD->HasInteractiveLaunchButtonForContract(), TEXT("A_LaunchButtonVisible"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Ferronite:")), TEXT("A_PlanetaryLabel"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Orbital:")), TEXT("A_OrbitalLabel"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Orbital: 0")), TEXT("B_OrbitalInitialZero"));
		Expect(!HUD->IsLaunchButtonEnabledForContract(), TEXT("E_DisabledNoReady"));

		UGP_AbilitySystemComponent* ASC = OwnPS->GetGPAbilitySystemComponent();
		BoundASCWeak = ASC;
		if (!Expect(ASC != nullptr && OwnPS->GetPlayerAttributeSet() != nullptr, TEXT("B_ASCPresent")))
		{
			Finish();
			return;
		}
		Expect(FMath::IsNearlyEqual(OwnPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 0.0f), TEXT("B_AttrInitialZero"));

		OrbitalAttrHandle = ASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).AddUObject(
			this, &UGP_ContainerLaunchHUDContractTestRunner::HandleOrbitalAttrChanged);

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(OwnPS);
		const FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(UGP_GE_AddOrbital::StaticClass(), 1.0f, Context);
		if (!Expect(Spec.IsValid(), TEXT("C_OrbitalGESpec")))
		{
			Finish();
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(UGP_GE_AddOrbital::GetMagnitudeDataName(), 25.0f);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

		Expect(OrbitalAttrEventCount >= 1, TEXT("C_OrbitalDelegateFired"));
		Expect(FMath::IsNearlyEqual(LastOrbitalAttrValue, 25.0f, 0.05f), TEXT("C_OrbitalDelegateValue"));
		Expect(FMath::IsNearlyEqual(HUD->GetDisplayedOrbitalForContract(), 25.0f, 0.05f), TEXT("C_HUDOrbitalUpdated"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Orbital: 25")), TEXT("C_HUDOrbitalText"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* OwnBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-47000.0f, 1200.0f, 100.0f), FRotator::ZeroRotator, Params);
		AGP_MainBase* OtherBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-47000.0f, 2200.0f, 100.0f), FRotator::ZeroRotator, Params);
		OwnBaseWeak = OwnBase;
		OtherBaseWeak = OtherBase;
		if (!Expect(IsValid(OwnBase) && IsValid(OtherBase), TEXT("SpawnMainBases")))
		{
			Finish();
			return;
		}
		OwnBase->SetTeamId(11);
		OtherBase->SetTeamId(12);

		UGP_StorageComponent* OwnStorage = OwnBase->GetStorageComponent();
		Expect(IsValid(OwnStorage), TEXT("OwnStoragePresent"));
		HUD->SetPlanetaryFerroniteDisplay(0.0f, true);
		HUD->SetLaunchButtonEnabled(OwnStorage->GetReadyCount() > 0 && !OwnStorage->IsLaunchInFlight());
		Expect(!HUD->IsLaunchButtonEnabledForContract(), TEXT("E_StillDisabledEmpty"));

		OwnStorage->AddPlanetaryFerronite(100.0f);
		Expect(OwnStorage->GetReadyCount() >= 1, TEXT("D_ReadyPresent"));
		HUD->SetPlanetaryFerroniteDisplay(OwnStorage->GetTotalStored(), true);
		HUD->SetLaunchButtonEnabled(OwnStorage->GetReadyCount() > 0 && !OwnStorage->IsLaunchInFlight());
		Expect(HUD->IsLaunchButtonEnabledForContract(), TEXT("D_EnabledWhenReady"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Ferronite: 100")), TEXT("A_PlanetaryReadyText"));

		OtherBase->GetStorageComponent()->AddPlanetaryFerronite(100.0f);
		Expect(OtherBase->GetStorageComponent()->GetReadyCount() >= 1, TEXT("OtherReadyPresent"));

		const bool bOtherLaunch = OtherPC->AuthorityTryLaunchReadyContainerForOwningTeam();
		Expect(bOtherLaunch, TEXT("H_OtherTeamLaunchesOwnBase"));
		Expect(OtherBase->GetStorageComponent()->IsLaunchInFlight()
			|| OtherBase->GetStorageComponent()->GetLaunchingCount() >= 1, TEXT("H_OtherLaunching"));
		Expect(OwnStorage->GetReadyCount() >= 1, TEXT("H_OwnReadyUntouchedByOther"));
		Expect(!OwnStorage->IsLaunchInFlight(), TEXT("H_OwnNotLaunching"));

		const bool bOwnLaunch = OwnPC->AuthorityTryLaunchReadyContainerForOwningTeam();
		Expect(bOwnLaunch, TEXT("G_OwnPCLaunchAccepted"));
		Expect(OwnStorage->IsLaunchInFlight(), TEXT("G_OwnLaunchInFlight"));
		Expect(OwnStorage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("G_LaunchingState"));
		HUD->SetLaunchButtonEnabled(OwnStorage->GetReadyCount() > 0 && !OwnStorage->IsLaunchInFlight());
		Expect(!HUD->IsLaunchButtonEnabledForContract(), TEXT("F_DisabledDuringLaunching"));

		const bool bSecond = OwnPC->AuthorityTryLaunchReadyContainerForOwningTeam();
		Expect(!bSecond, TEXT("G_SecondLaunchRejected"));

		float Duration = 2.5f;
		if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
		{
			Duration = Settings->ContainerLaunchDurationSeconds;
		}
		++StageIndex;
		ScheduleNext(Duration + 0.2f);
		break;
	}
	case 1:
	{
		UGP_StorageComponent* OwnStorage = OwnBaseWeak.IsValid() ? OwnBaseWeak->GetStorageComponent() : nullptr;
		UGP_TEMP_S28P_PlanetaryFerroniteHUD* HUD = HUDWeak.Get();
		AGP_PlayerState* OwnPS = OwnPSWeak.Get();
		if (!Expect(IsValid(OwnStorage) && IsValid(HUD) && IsValid(OwnPS), TEXT("CompletionActorsAlive")))
		{
			Finish();
			return;
		}

		Expect(!OwnStorage->IsLaunchInFlight(), TEXT("Completion_Finished"));
		Expect(OwnStorage->GetContainers()[0].State == EGP_StorageContainerState::Empty, TEXT("Completion_Empty"));
		Expect(FMath::IsNearlyEqual(OwnPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 125.0f, 0.05f),
			TEXT("Completion_OrbitalGranted"));

		HUD->SetPlanetaryFerroniteDisplay(OwnStorage->GetTotalStored(), true);
		HUD->SetOrbitalFerroniteDisplay(OwnPS->GetPlayerAttributeSet()->GetOrbitalFerronite());
		HUD->SetLaunchButtonEnabled(OwnStorage->GetReadyCount() > 0 && !OwnStorage->IsLaunchInFlight());
		Expect(!HUD->IsLaunchButtonEnabledForContract(), TEXT("Completion_DisabledNoReady"));
		Expect(HUD->GetCountersDisplayTextForContract().Contains(TEXT("Orbital: 125")), TEXT("Completion_OrbitalHUD"));

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

void UGP_ContainerLaunchHUDContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_ContainerLaunchHUDContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_ContainerLaunchHUDContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_ContainerLaunchHUDContractTestRunner::AdvanceStage() {}
bool UGP_ContainerLaunchHUDContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_ContainerLaunchHUDContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_ContainerLaunchHUDContractTestRunner::Finish() { bFinished = true; }
void UGP_ContainerLaunchHUDContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_ContainerLaunchHUDContractTestRunner::UnbindWorldCleanup() {}
void UGP_ContainerLaunchHUDContractTestRunner::CleanupActors() {}
void UGP_ContainerLaunchHUDContractTestRunner::HandleOrbitalAttrChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;
}

#endif // !UE_BUILD_SHIPPING
