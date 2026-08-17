// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPUnitCapLogisticsHubContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Combat/GPDamageApplication.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Effects/GPGE_DamageBasic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitCapHub, Log, All);

namespace GPUnitCapHubDebug
{
	static TWeakObjectPtr<UGP_UnitCapLogisticsHubContractTestRunner> GActiveRunner;
	constexpr int32 TeamA = 91;
	constexpr int32 TeamB = 92;

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

	static void GrantOrbital(AGP_PlayerState* PS, float Amount)
	{
		if (!IsValid(PS) || Amount <= 0.0f)
		{
			return;
		}
		UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(PS);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGP_GE_AddOrbital::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(UGP_GE_AddOrbital::GetMagnitudeDataName(), Amount);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	static int32 CurrentUnits(const AGP_PlayerState* PS)
	{
		if (PS == nullptr || PS->GetPlayerAttributeSet() == nullptr)
		{
			return -1;
		}
		return FMath::RoundToInt(PS->GetPlayerAttributeSet()->GetCurrentUnits());
	}

	static int32 MaxUnits(const AGP_PlayerState* PS)
	{
		if (PS == nullptr || PS->GetPlayerAttributeSet() == nullptr)
		{
			return -1;
		}
		return FMath::RoundToInt(PS->GetPlayerAttributeSet()->GetMaxUnits());
	}

	static AGP_Worker* SpawnOwnedWorker(UWorld* World, AGP_PlayerState* PS, int32 TeamId, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = PS;
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(AGP_Worker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (IsValid(Worker))
		{
			Worker->SetTeamId(TeamId);
		}
		return Worker;
	}

	static bool KillUnit(AGP_UnitBase* Unit)
	{
		if (!IsValid(Unit))
		{
			return false;
		}
		UGP_UnitAttributeSet* Attr = const_cast<UGP_UnitAttributeSet*>(Unit->GetUnitAttributeSet());
		UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		if (Attr == nullptr || ASC == nullptr)
		{
			return false;
		}
		Attr->SetDamage(FMath::Max(Attr->GetMaxHealth(), 1.0f) + 1000.0f);
		FGP_DamageApplicationResult Result;
		GPDamageApplication::ApplyDamageEffect(ASC, ASC, UGP_GE_Damage_Basic::StaticClass(), Result);
		return Unit->IsDead();
	}

	static void DestroyTeamUnits(UWorld* World, int32 TeamId)
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == TeamId)
			{
				ToDestroy.Add(*It);
			}
		}
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == TeamId)
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}

	static void RunUnitCapLogisticsHubContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPUnitCapHub, Warning, TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPUnitCapHub, Warning, TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("UnitCapLogisticsHubContract"), TEXT("UnitCapLogisticsHub"), Token))
		{
			return;
		}

		UGP_UnitCapLogisticsHubContractTestRunner* Runner =
			NewObject<UGP_UnitCapLogisticsHubContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GUnitCapHubContract(
		TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest"),
		TEXT("Authority: GP-S33C unit cap + Logistics Hub capacity contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunUnitCapLogisticsHubContractTest));
}

void UGP_UnitCapLogisticsHubContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_UnitCapLogisticsHubContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_UnitCapLogisticsHubContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_UnitCapLogisticsHubContractTestRunner::RestoreSettings()
{
	if (!bSettingsMutated)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		Settings->UnitDropDescentDurationSeconds = SavedDescent;
		Settings->UnitDropCleanupDelaySeconds = SavedCleanup;
		Settings->UnitDropSpawnAltitudeCm = SavedAltitude;
		Settings->UnitDropPayloadDeployDelaySeconds = SavedDeployDelay;
		Settings->BuildingDropDescentDurationSeconds = SavedBuildingDescent;
		Settings->BuildingDropPayloadDeployDelaySeconds = SavedBuildingDeployDelay;
		Settings->BuildingDropCleanupDelaySeconds = SavedBuildingCleanup;
	}
	bSettingsMutated = false;
}

void UGP_UnitCapLogisticsHubContractTestRunner::CleanupActors()
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
		GPUnitCapHubDebug::DestroyTeamUnits(World, GPUnitCapHubDebug::TeamA);
		GPUnitCapHubDebug::DestroyTeamUnits(World, GPUnitCapHubDebug::TeamB);
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPUnitCapHubDebug::TeamA || It->GetTeamId() == GPUnitCapHubDebug::TeamB)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
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
	DestroyWeak(LastPodWeak);
	DestroyWeak(HubAWeak);
	DestroyWeak(HubBWeak);
	DestroyWeak(MainBaseWeak);
	DestroyWeak(OwnerPSWeak);
	DestroyWeak(OtherPSWeak);
}

void UGP_UnitCapLogisticsHubContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	RestoreSettings();
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	UnbindWorldCleanup();
	CleanupActors();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPUnitCapHub, Log,
		TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));
	RemoveFromRoot();
	GPUnitCapHubDebug::GActiveRunner.Reset();
}

void UGP_UnitCapLogisticsHubContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPUnitCapHub, Error,
		TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_UnitCapLogisticsHubContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPUnitCapHub, Error,
			TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPUnitCapHub, Log,
		TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest PASS: %s"), Label);
	return true;
}

void UGP_UnitCapLogisticsHubContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_UnitCapLogisticsHubContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_UnitCapLogisticsHubContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_UnitCapLogisticsHubContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPUnitCapHub, Log, TEXT("gp.Resource.RunUnitCapLogisticsHubContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_UnitCapLogisticsHubContractTestRunner::AdvanceStage()
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

	const FVector Origin(-52000.0f, 8000.0f, 100.0f);

	switch (StageIndex)
	{
	case 0: // A base MaxUnits=5 + B two preplaced workers
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedDescent = Settings->UnitDropDescentDurationSeconds;
			SavedCleanup = Settings->UnitDropCleanupDelaySeconds;
			SavedAltitude = Settings->UnitDropSpawnAltitudeCm;
			SavedDeployDelay = Settings->UnitDropPayloadDeployDelaySeconds;
			SavedBuildingDescent = Settings->BuildingDropDescentDurationSeconds;
			SavedBuildingDeployDelay = Settings->BuildingDropPayloadDeployDelaySeconds;
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			Settings->UnitDropDescentDurationSeconds = 0.35f;
			Settings->UnitDropCleanupDelaySeconds = 0.05f;
			Settings->UnitDropSpawnAltitudeCm = 400.0f;
			Settings->UnitDropPayloadDeployDelaySeconds = 0.15f;
			Settings->BuildingDropDescentDurationSeconds = 0.30f;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.15f;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->WorkerPayloadClass.Reset();
			Settings->SalvageWalkerPayloadClass.Reset();
			Settings->UnitDropPodClass.Reset();
			Settings->BuildingPayloadClass.Reset();
			bSettingsMutated = true;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base) && IsValid(Base->GetUnitDropZone()), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(GPUnitCapHubDebug::TeamA);

		AGP_PlayerState* OwnerPS = GPUnitCapHubDebug::SpawnTeamPlayerState(World, GS, GPUnitCapHubDebug::TeamA);
		OwnerPSWeak = OwnerPS;
		AGP_PlayerState* OtherPS = GPUnitCapHubDebug::SpawnTeamPlayerState(World, GS, GPUnitCapHubDebug::TeamB);
		OtherPSWeak = OtherPS;
		if (!Expect(IsValid(OwnerPS) && IsValid(OtherPS), TEXT("SpawnPlayerStates")))
		{
			Finish();
			return;
		}

		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("A_BaseMaxUnits5"));
		Expect(GPUnitCapHubDebug::MaxUnits(OtherPS) == 5, TEXT("L_OtherBaseMaxUnits5"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 0, TEXT("A_BaseCurrent0"));

		GPUnitCapHubDebug::SpawnOwnedWorker(World, OwnerPS, GPUnitCapHubDebug::TeamA, Origin + FVector(200.0f, 0.0f, 0.0f));
		GPUnitCapHubDebug::SpawnOwnedWorker(World, OwnerPS, GPUnitCapHubDebug::TeamA, Origin + FVector(280.0f, 0.0f, 0.0f));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 2, TEXT("B_PreplacedCurrent2"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OtherPS) == 0, TEXT("L_OtherUnaffectedByAUnits"));

		GPUnitCapHubDebug::GrantOrbital(OwnerPS, 2000.0f);
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1: // C death 2→1
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("C_OwnerAlive")))
		{
			Finish();
			return;
		}

		AGP_Worker* Victim = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPUnitCapHubDebug::TeamA && !It->IsDead())
			{
				Victim = *It;
				break;
			}
		}
		if (!Expect(IsValid(Victim), TEXT("C_VictimPresent")))
		{
			Finish();
			return;
		}
		Expect(GPUnitCapHubDebug::KillUnit(Victim), TEXT("C_KillApplied"));
		Expect(Victim->IsDead(), TEXT("C_IsDead"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 1, TEXT("C_Current2to1"));
		Expect(GPUnitCapHubDebug::KillUnit(Victim) == false || GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 1,
			TEXT("C_NoDoubleDecrement"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 1, TEXT("C_CurrentStill1"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2: // Fill to 4, D/F reservation race
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("D_OwnerAlive")))
		{
			Finish();
			return;
		}

		GPUnitCapHubDebug::SpawnOwnedWorker(World, OwnerPS, GPUnitCapHubDebug::TeamA, Origin + FVector(360.0f, 0.0f, 0.0f));
		GPUnitCapHubDebug::SpawnOwnedWorker(World, OwnerPS, GPUnitCapHubDebug::TeamA, Origin + FVector(440.0f, 0.0f, 0.0f));
		GPUnitCapHubDebug::SpawnOwnedWorker(World, OwnerPS, GPUnitCapHubDebug::TeamA, Origin + FVector(520.0f, 0.0f, 0.0f));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 4, TEXT("D_Current4"));
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("D_Max5"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 0, TEXT("D_Pending0"));

		FGP_UnitDropManifest Two;
		Two.WorkerCount = 2;
		GPUnitDropAuthority::FEvalResult TwoResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Two);
		Expect(!TwoResult.bAccepted, TEXT("D_TwoUnitReject"));
		Expect(TwoResult.RejectReason == EGP_UnitDropRejectReason::UnitCapReached, TEXT("D_TwoUnitCapReason"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 0, TEXT("D_RejectNoReserve"));

		FGP_UnitDropManifest One;
		One.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult OneResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, One);
		Expect(OneResult.bAccepted, TEXT("D_OneUnitAccept"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 1, TEXT("F_PendingReserved"));
		LastPodWeak = OneResult.SpawnedPod;

		GPUnitDropAuthority::FEvalResult RaceResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, One);
		Expect(!RaceResult.bAccepted, TEXT("F_SecondOrderReject"));
		Expect(RaceResult.RejectReason == EGP_UnitDropRejectReason::UnitCapReached, TEXT("F_SecondOrderCapReason"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 1, TEXT("F_PendingStill1"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 4, TEXT("F_CurrentStill4BeforeLand"));

		++StageIndex;
		ScheduleNext(0.70f);
		break;
	}
	case 3: // Payload live converts reservation
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("F_OwnerAliveAfterLand")))
		{
			Finish();
			return;
		}
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 5, TEXT("F_Current5AfterLand"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 0, TEXT("F_ReservationCleared"));

		GPUnitCapHubDebug::DestroyTeamUnits(World, GPUnitCapHubDebug::TeamA);
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 0, TEXT("E_CurrentCleared"));

		FGP_UnitDropManifest Walker;
		Walker.SalvageWalkerCount = 1;
		GPUnitDropAuthority::FEvalResult WalkerResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Walker);
		Expect(WalkerResult.bAccepted, TEXT("E_WalkerAccept"));
		Expect(WalkerResult.SlotCost == 2, TEXT("E_WalkerSlots2"));
		Expect(WalkerResult.UnitCount == 1, TEXT("E_WalkerEntityCount1"));
		LastPodWeak = WalkerResult.SpawnedPod;

		++StageIndex;
		ScheduleNext(0.70f);
		break;
	}
	case 4: // E walker counts 1 + G failed payload
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("E_OwnerAlive")))
		{
			Finish();
			return;
		}
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 1, TEXT("E_WalkerCurrent1Not2"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 0, TEXT("E_WalkerPendingCleared"));

		GPUnitCapHubDebug::DestroyTeamUnits(World, GPUnitCapHubDebug::TeamA);
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 0, TEXT("G_Current0BeforeFail"));

		FGP_UnitDropManifest FailOne;
		FailOne.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult FailResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, FailOne);
		Expect(FailResult.bAccepted, TEXT("G_OrderAccepted"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 1, TEXT("G_PendingReserved"));
		AGP_DropPod* FailPod = FailResult.SpawnedPod.Get();
		if (Expect(IsValid(FailPod), TEXT("G_PodPresent")))
		{
			FailPod->DebugForceSkipPayloadSpawn();
		}
		LastPodWeak = FailPod;

		++StageIndex;
		ScheduleNext(0.70f);
		break;
	}
	case 5: // G reservation recovered + H purchase READY does not raise Max
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("G_OwnerAlive")))
		{
			Finish();
			return;
		}
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 0, TEXT("G_CurrentUnchanged"));
		Expect(OwnerPS->GetPendingOrbitalUnitCount() == 0, TEXT("G_ReservationReleased"));
		int32 Workers = 0;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPUnitCapHubDebug::TeamA)
			{
				++Workers;
			}
		}
		Expect(Workers == 0, TEXT("G_NoPayloadWorkers"));

		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("H_Max5BeforePurchase"));
		GPBuildingDropAuthority::FPurchaseResult Purchase =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub);
		Expect(Purchase.bAccepted, TEXT("H_PurchaseAccept"));
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("H_ReadyDoesNotRaiseMax"));

		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(Base), TEXT("H_BaseAlive")))
		{
			Finish();
			return;
		}
		const FTransform DeployXform(FRotator::ZeroRotator, Base->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f));
		GPBuildingDropAuthority::FDeployResult Deploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub, DeployXform);
		Expect(Deploy.bAccepted, TEXT("H_DeployAccept"));
		LastPodWeak = Deploy.SpawnedPod;
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("H_MaxStill5WhileDescending"));

		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 6: // Still descending / deploying — Max still 5
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_DropPod* Pod = LastPodWeak.Get();
		Expect(IsValid(OwnerPS), TEXT("H_OwnerDuringDescent"));
		if (IsValid(Pod))
		{
			Expect(Pod->GetPhase() != EGP_DropPodPhase::PayloadDeployed, TEXT("H_NotPayloadYet"));
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("H_Max5BeforeHubLive"));
		++StageIndex;
		ScheduleNext(0.50f);
		break;
	}
	case 7: // Hub live 5→10, stack second hub 15, isolation
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_PlayerState* OtherPS = OtherPSWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(OtherPS), TEXT("H_PlayersAlive")))
		{
			Finish();
			return;
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 10, TEXT("H_Max10WhenHubLive"));
		Expect(GPUnitCapHubDebug::MaxUnits(OtherPS) == 5, TEXT("L_PlayerBUnchangedByAHub"));

		AGP_LogisticsHub* LiveHub = nullptr;
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPUnitCapHubDebug::TeamA && !It->IsDead())
			{
				LiveHub = *It;
				break;
			}
		}
		HubAWeak = LiveHub;
		Expect(IsValid(LiveHub), TEXT("H_HubActorPresent"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = OwnerPS;
		Params.ObjectFlags |= RF_Transient;
		AGP_LogisticsHub* HubB = World->SpawnActor<AGP_LogisticsHub>(
			AGP_LogisticsHub::StaticClass(),
			Origin + FVector(800.0f, 500.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		HubBWeak = HubB;
		if (Expect(IsValid(HubB), TEXT("I_SecondHubSpawned")))
		{
			HubB->SetTeamId(GPUnitCapHubDebug::TeamA);
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 15, TEXT("I_Max15Stacked"));
		Expect(GPUnitCapHubDebug::MaxUnits(OtherPS) == 5, TEXT("L_PlayerBStill5"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8: // J destroy 15→10→5 no double remove
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("J_OwnerAlive")))
		{
			Finish();
			return;
		}

		AGP_LogisticsHub* HubB = HubBWeak.Get();
		Expect(GPUnitCapHubDebug::KillUnit(HubB), TEXT("J_KillHubB"));
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 10, TEXT("J_Max15to10"));
		if (IsValid(HubB))
		{
			HubB->Destroy();
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 10, TEXT("J_NoDoubleRemoveHubB"));

		AGP_LogisticsHub* HubA = HubAWeak.Get();
		Expect(GPUnitCapHubDebug::KillUnit(HubA), TEXT("J_KillHubA"));
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("J_Max10to5"));
		if (IsValid(HubA))
		{
			HubA->Destroy();
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("J_NoDoubleRemoveHubA"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9: // K over-cap destroy: Current 8 / Max 10 → Max 5 Current 8
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("K_OwnerAlive")))
		{
			Finish();
			return;
		}

		GPUnitCapHubDebug::DestroyTeamUnits(World, GPUnitCapHubDebug::TeamA);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = OwnerPS;
		Params.ObjectFlags |= RF_Transient;
		AGP_LogisticsHub* Hub = World->SpawnActor<AGP_LogisticsHub>(
			AGP_LogisticsHub::StaticClass(),
			Origin + FVector(900.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		HubAWeak = Hub;
		if (Expect(IsValid(Hub), TEXT("K_HubSpawned")))
		{
			Hub->SetTeamId(GPUnitCapHubDebug::TeamA);
		}
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 10, TEXT("K_Max10"));

		for (int32 i = 0; i < 8; ++i)
		{
			GPUnitCapHubDebug::SpawnOwnedWorker(
				World,
				OwnerPS,
				GPUnitCapHubDebug::TeamA,
				Origin + FVector(200.0f + 80.0f * static_cast<float>(i), 200.0f, 0.0f));
		}
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 8, TEXT("K_Current8"));

		Expect(GPUnitCapHubDebug::KillUnit(Hub), TEXT("K_DestroyHub"));
		Expect(GPUnitCapHubDebug::MaxUnits(OwnerPS) == 5, TEXT("K_Max5AfterDestroy"));
		Expect(GPUnitCapHubDebug::CurrentUnits(OwnerPS) == 8, TEXT("K_CurrentRemains8"));
		int32 Living = 0;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPUnitCapHubDebug::TeamA && !It->IsDead())
			{
				++Living;
			}
		}
		Expect(Living == 8, TEXT("K_UnitsRemainAlive"));

		FGP_UnitDropManifest One;
		One.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult Reject =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, One);
		Expect(!Reject.bAccepted, TEXT("K_NewOrderRejected"));
		Expect(Reject.RejectReason == EGP_UnitDropRejectReason::UnitCapReached, TEXT("K_RejectUnitCapReached"));

		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_UnitCapLogisticsHubContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_UnitCapLogisticsHubContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_UnitCapLogisticsHubContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_UnitCapLogisticsHubContractTestRunner::AdvanceStage() {}
bool UGP_UnitCapLogisticsHubContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_UnitCapLogisticsHubContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_UnitCapLogisticsHubContractTestRunner::Finish() { bFinished = true; }
void UGP_UnitCapLogisticsHubContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_UnitCapLogisticsHubContractTestRunner::UnbindWorldCleanup() {}
void UGP_UnitCapLogisticsHubContractTestRunner::CleanupActors() {}
void UGP_UnitCapLogisticsHubContractTestRunner::RestoreSettings() {}

#endif
