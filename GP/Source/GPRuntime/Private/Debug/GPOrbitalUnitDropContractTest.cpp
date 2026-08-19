// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalUnitDropContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Components/SceneComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Orbital/GPUnitGroundPlacement.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPOrbitalUnitDrop, Log, All);

namespace GPOrbitalUnitDropDebug
{
	static TWeakObjectPtr<UGP_OrbitalUnitDropContractTestRunner> GActiveRunner;

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

	static void RunOrbitalUnitDropContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPOrbitalUnitDrop, Warning, TEXT("GP Resource.RunOrbitalUnitDropContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPOrbitalUnitDrop, Warning, TEXT("GP Resource.RunOrbitalUnitDropContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("OrbitalUnitDropContract"), TEXT("OrbitalUnitDrop"), Token))
		{
			return;
		}

		UGP_OrbitalUnitDropContractTestRunner* Runner =
			NewObject<UGP_OrbitalUnitDropContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GOrbitalUnitDropContract(
		TEXT("gp.Resource.RunOrbitalUnitDropContractTest"),
		TEXT("Authority: GP-S31R orbital unit drop contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunOrbitalUnitDropContractTest));
}

void UGP_OrbitalUnitDropContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_OrbitalUnitDropContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_OrbitalUnitDropContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_OrbitalUnitDropContractTestRunner::RestoreSettings()
{
	if (UGP_OrbitalUnitDropCatalog* Existing = UGP_OrbitalUnitDropCatalog::TryGetExisting())
	{
		Existing->DebugClearAuthoredUnitDropOverrides();
		Existing->DebugEndContractIsolation();
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (bSettingsMutated)
		{
			Settings->UnitDropDescentDurationSeconds = SavedDescent;
			Settings->UnitDropCleanupDelaySeconds = SavedCleanup;
			Settings->UnitDropSpawnAltitudeCm = SavedAltitude;
			Settings->UnitDropPayloadDeployDelaySeconds = SavedDeployDelay;
			Settings->UnitDropPodClass = SavedDropPodClass;
			if (UGP_OrbitalUnitDropCatalog* Existing = UGP_OrbitalUnitDropCatalog::TryGetExisting())
			{
				Existing->OverrideDeliveryTiming(2.5f, 1.25f);
			}
		}
		Settings->WorkerDropDefinition = SavedWorkerDropDef;
		Settings->SalvageWalkerDropDefinition = SavedWalkerDropDef;
	}
	if (UGP_OrbitalUnitDropCatalog* Existing = UGP_OrbitalUnitDropCatalog::TryGetExisting())
	{
		Existing->RefreshAuthoredBindings();
	}
	bSettingsMutated = false;
}

void UGP_OrbitalUnitDropContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (AGP_PlayerState* OwnerPS = OwnerPSWeak.Get())
			{
				GS->RemovePlayerState(OwnerPS);
			}
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == 71)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == 71)
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
	DestroyWeak(MainBaseWeak);
	DestroyWeak(OwnerPSWeak);
	AuthoredWorkerDropDef = nullptr;
	AuthoredWorkerUnitDef = nullptr;
}

void UGP_OrbitalUnitDropContractTestRunner::Finish()
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
	UE_LOG(LogGPOrbitalUnitDrop, Log,
		TEXT("GP Resource.RunOrbitalUnitDropContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));
	RemoveFromRoot();
	GPOrbitalUnitDropDebug::GActiveRunner.Reset();
}

void UGP_OrbitalUnitDropContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPOrbitalUnitDrop, Error,
		TEXT("GP Resource.RunOrbitalUnitDropContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_OrbitalUnitDropContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPOrbitalUnitDrop, Error,
			TEXT("GP Resource.RunOrbitalUnitDropContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPOrbitalUnitDrop, Log,
		TEXT("GP Resource.RunOrbitalUnitDropContractTest PASS: %s"), Label);
	return true;
}

void UGP_OrbitalUnitDropContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_OrbitalUnitDropContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_OrbitalUnitDropContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_OrbitalUnitDropContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPOrbitalUnitDrop, Log, TEXT("GP Resource.RunOrbitalUnitDropContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_OrbitalUnitDropContractTestRunner::AdvanceStage()
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

	constexpr int32 ContractTeam = 71;

	switch (StageIndex)
	{
	case 0: // Setup + reject paths A–D
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedDescent = Settings->UnitDropDescentDurationSeconds;
			SavedCleanup = Settings->UnitDropCleanupDelaySeconds;
			SavedAltitude = Settings->UnitDropSpawnAltitudeCm;
			SavedDeployDelay = Settings->UnitDropPayloadDeployDelaySeconds;
			SavedDropPodClass = Settings->UnitDropPodClass;
			SavedWorkerDropDef = Settings->WorkerDropDefinition;
			SavedWalkerDropDef = Settings->SalvageWalkerDropDefinition;
			Settings->UnitDropDescentDurationSeconds = 0.25f;
			Settings->UnitDropCleanupDelaySeconds = 0.05f;
			Settings->UnitDropSpawnAltitudeCm = 400.0f;
			Settings->UnitDropPayloadDeployDelaySeconds = 0.0f; // zero-delay path for core flow
			UGP_OrbitalUnitDropCatalog::Get().OverrideDeliveryTiming(
				Settings->UnitDropDescentDurationSeconds,
				Settings->UnitDropPayloadDeployDelaySeconds);
			Settings->UnitDropPodClass.Reset();
			bSettingsMutated = true;

			UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
			UnitDrops.DebugBeginContractIsolation();
			const UClass* SettingsClass = UGP_OrbitalDeliverySettings::StaticClass();
			bool bUsedAuthored = true;
			Expect(SettingsClass->FindPropertyByName(TEXT("WorkerPayloadClass")) == nullptr
				&& SettingsClass->FindPropertyByName(TEXT("SalvageWalkerPayloadClass")) == nullptr
				&& UnitDrops.ResolveWorkerPayloadClass() == AGP_Worker::StaticClass()
				&& UnitDrops.ResolveSalvageWalkerPayloadClass() == AGP_SalvageWalker::StaticClass(),
				TEXT("Payload_UnconfiguredNativeAndSettingsAbsent"));
			Expect(Settings->ResolveUnitDropPodClass(&bUsedAuthored) == AGP_DropPod::StaticClass()
				&& !bUsedAuthored, TEXT("F_FallbackNativePod"));
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-47000.0f, 2000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base) && IsValid(Base->GetUnitDropZone()), TEXT("SpawnMainBaseDropZone")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(ContractTeam);

		AGP_PlayerState* OwnerPS = GPOrbitalUnitDropDebug::SpawnTeamPlayerState(World, GS, ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS), TEXT("SpawnOwnerPS")))
		{
			Finish();
			return;
		}

		FGP_UnitDropManifest Empty;
		GPUnitDropAuthority::FEvalResult EmptyResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Empty);
		Expect(!EmptyResult.bAccepted, TEXT("A_EmptyReject"));
		Expect(EmptyResult.RejectReason == EGP_UnitDropRejectReason::EmptyManifest, TEXT("A_EmptyReason"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 0.0f), TEXT("A_NoSpend"));

		FGP_UnitDropManifest Overflow;
		Overflow.WorkerCount = 5; // 5 > capacity 4
		GPUnitDropAuthority::FEvalResult OverflowResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Overflow);
		Expect(!OverflowResult.bAccepted, TEXT("B_SlotOverflowReject"));
		Expect(OverflowResult.RejectReason == EGP_UnitDropRejectReason::SlotOverflow, TEXT("B_SlotOverflowReason"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 0.0f), TEXT("B_NoSpend"));

		FGP_UnitDropManifest Poor;
		Poor.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult PoorResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Poor);
		Expect(!PoorResult.bAccepted, TEXT("C_InsufficientOrbitalReject"));
		Expect(PoorResult.RejectReason == EGP_UnitDropRejectReason::InsufficientOrbital, TEXT("C_InsufficientReason"));

		// Missing DropZone
		if (USceneComponent* Zone = Base->GetUnitDropZone())
		{
			Zone->DestroyComponent();
		}
		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		FGP_UnitDropManifest MissingZone;
		MissingZone.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult MissingZoneResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, MissingZone);
		Expect(!MissingZoneResult.bAccepted, TEXT("D_MissingDropZoneReject"));
		Expect(MissingZoneResult.RejectReason == EGP_UnitDropRejectReason::MissingDropZone, TEXT("D_MissingDropZoneReason"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), 100.0f, 0.05f), TEXT("D_NoSpend"));

		// Recreate base with zone for accept paths
		Base->Destroy();
		MainBaseWeak.Reset();
		Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-47000.0f, 2000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base) && IsValid(Base->GetUnitDropZone()), TEXT("RespawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(ContractTeam);

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1: // E/F/G valid Worker + mixed, one pod
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("E_ActorsAlive")))
		{
			Finish();
			return;
		}

		// Ensure funded (may already be 100 from D)
		if (OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite() < 100.0f)
		{
			GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		}
		OrbitalBeforeSpend = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();

		FGP_UnitDropManifest Workers;
		Workers.WorkerCount = 2;
		GPUnitDropAuthority::FEvalResult WorkerResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Workers);
		Expect(WorkerResult.bAccepted, TEXT("E_WorkerAccept"));
		Expect(WorkerResult.UnitCount == 2, TEXT("E_WorkerCount"));
		Expect(WorkerResult.SlotCost == 2, TEXT("E_WorkerSlots"));
		Expect(FMath::IsNearlyEqual(WorkerResult.OrbitalCost, 50.0f, 0.05f), TEXT("E_WorkerCost"));
		Expect(IsValid(WorkerResult.SpawnedPod.Get()), TEXT("E_PodSpawned"));
		Expect(WorkerResult.SpawnedPod->IsA(AGP_DropPod::StaticClass()), TEXT("E_PodIsDropPod"));
		Expect(WorkerResult.SpawnedPod->GetClass() == AGP_DropPod::StaticClass(), TEXT("F_NativePodWhenSoftEmpty"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBeforeSpend - 50.0f,
			0.05f), TEXT("E_SpendOnce"));
		LastPodWeak = WorkerResult.SpawnedPod;

		int32 PodCount = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			++PodCount;
		}
		Expect(PodCount == 1, TEXT("G_ExactlyOnePod"));

		++StageIndex;
		ScheduleNext(0.35f); // wait landing
		break;
	}
	case 2: // H/I/J/K landing spawn checks + mixed + duplicate
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("H_OwnerAlive")))
		{
			Finish();
			return;
		}

		TArray<AGP_Worker*> Workers;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				Workers.Add(*It);
			}
		}
		Expect(Workers.Num() == 2, TEXT("H_WorkerSpawnCount"));
		if (Workers.Num() >= 2)
		{
			Expect(Workers[0]->GetActorLocation().Equals(Workers[1]->GetActorLocation(), 1.0f) == false,
				TEXT("K_OffsetsDistinct"));
			Expect(Workers[0]->GetTeamId() == ContractTeam, TEXT("J_WorkerTeamId"));
		}

		AGP_MainBase* BaseForGround = MainBaseWeak.Get();
		if (Expect(IsValid(BaseForGround) && IsValid(BaseForGround->GetUnitDropZone()), TEXT("Ground_DropZone")))
		{
			const float GroundZ = BaseForGround->GetUnitDropZone()->GetComponentLocation().Z;
			for (AGP_Worker* W : Workers)
			{
				if (!IsValid(W))
				{
					continue;
				}
				const float HalfH = GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(W->GetClass());
				const float BottomZ = W->GetActorLocation().Z - HalfH;
				Expect(FMath::IsNearlyEqual(BottomZ, GroundZ, 8.0f), TEXT("Ground_WorkerCapsuleBottom"));
				Expect(HalfH > 1.0f, TEXT("Ground_WorkerHalfHeightFromClass"));
			}
		}

		const float OrbitalAfterWorkers = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();

		FGP_UnitDropManifest Mixed;
		Mixed.WorkerCount = 2;
		Mixed.SalvageWalkerCount = 1; // slots 2+2=4, cost 50+50=100 — need funds
		if (OrbitalAfterWorkers + KINDA_SMALL_NUMBER < 100.0f)
		{
			GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f - OrbitalAfterWorkers);
		}
		const float BeforeMixed = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		int32 SlotCost = 0;
		float OrbitalCost = 0.0f;
		int32 UnitCount = 0;
		EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
		Expect(GPUnitDropAuthority::ComputeManifestCosts(Mixed, SlotCost, OrbitalCost, UnitCount, Reject), TEXT("F_MixedComputeOk"));
		Expect(SlotCost == 4, TEXT("F_MixedSlots"));
		Expect(FMath::IsNearlyEqual(OrbitalCost, 100.0f, 0.05f), TEXT("F_MixedCost"));

		GPUnitDropAuthority::FEvalResult MixedResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Mixed);
		Expect(MixedResult.bAccepted, TEXT("F_MixedAccept"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			BeforeMixed - 100.0f,
			0.05f), TEXT("F_MixedSpendOnce"));

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 3: // Mixed landing + duplicate reject + unit cap + S30 launch
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("I_ActorsAlive")))
		{
			Finish();
			return;
		}

		int32 WalkerCount = 0;
		int32 WorkerCount = 0;
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				++WalkerCount;
				Expect(It->GetTeamId() == ContractTeam, TEXT("I_SWTeam"));
			}
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				++WorkerCount;
			}
		}
		Expect(WalkerCount == 1, TEXT("I_SWCount"));
		Expect(WorkerCount == 4, TEXT("I_TotalWorkersAfterMixed")); // 2 + 2

		if (AGP_MainBase* BaseGround = MainBaseWeak.Get())
		{
			if (USceneComponent* Zone = BaseGround->GetUnitDropZone())
			{
				const float GroundZ = Zone->GetComponentLocation().Z;
				for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
				{
					if (It->GetTeamId() != ContractTeam)
					{
						continue;
					}
					const float HalfH = GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(It->GetClass());
					const float BottomZ = It->GetActorLocation().Z - HalfH;
					Expect(FMath::IsNearlyEqual(BottomZ, GroundZ, 8.0f), TEXT("Ground_SWCapsuleBottom"));
				}
			}
		}

		const float BeforeDup = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		FGP_UnitDropManifest Dup;
		Dup.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult DupResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Dup);
		Expect(!DupResult.bAccepted, TEXT("L_DuplicateInsufficientReject"));
		Expect(DupResult.RejectReason == EGP_UnitDropRejectReason::InsufficientOrbital, TEXT("L_DuplicateReason"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), BeforeDup, 0.05f), TEXT("L_NoDoubleSpend"));

		// Unit cap when MaxUnits active
		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		FGP_UnitDropManifest Cap;
		Cap.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult CapResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Cap);
		Expect(!CapResult.bAccepted, TEXT("M_UnitCapReject"));
		Expect(CapResult.RejectReason == EGP_UnitDropRejectReason::UnitCapReached, TEXT("M_UnitCapReason"));

		// N: GP-S30 launch path still works (rewards apply on launch completion, not accept).
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (!Expect(IsValid(Storage), TEXT("N_StoragePresent")))
		{
			Finish();
			return;
		}
		Storage->AddPlanetaryFerronite(100.0f);
		Expect(Storage->GetReadyCount() >= 1, TEXT("N_ReadyPresent"));
		OrbitalBeforeSpend = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		FGP_ContainerLaunchResult Launch = Storage->TryLaunchReadyContainer();
		Expect(Launch.bAccepted, TEXT("N_LaunchAccepted"));
		Expect(Storage->IsLaunchInFlight(), TEXT("N_LaunchInFlight"));

		float Duration = Launch.LaunchDurationSeconds;
		if (const UGP_ResourceGameplaySettings* ResSettings = UGP_ResourceGameplaySettings::Get())
		{
			Duration = ResSettings->ContainerLaunchDurationSeconds;
		}
		++StageIndex;
		ScheduleNext(Duration + 0.15f);
		break;
	}
	case 4: // N completion + authored payload/pod soft-class seams
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("Authored_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (Expect(IsValid(Storage), TEXT("N_StorageAlive")))
		{
			Expect(!Storage->IsLaunchInFlight(), TEXT("N_LaunchFinished"));
		}
		Expect(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite() > OrbitalBeforeSpend + KINDA_SMALL_NUMBER,
			TEXT("N_LaunchGrantedOrbital"));

		UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>();
		if (!Expect(Settings != nullptr, TEXT("Authored_Settings")))
		{
			Finish();
			return;
		}

		// D: manifest is counts-only (no class field for client to choose)
		FGP_UnitDropManifest CountsOnly;
		CountsOnly.WorkerCount = 1;
		CountsOnly.SalvageWalkerCount = 0;
		Expect(CountsOnly.GetTotalUnitCount() == 1, TEXT("D_ManifestCountsOnly"));

		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UGP_UnitDefinitionCatalog& Units = UGP_UnitDefinitionCatalog::Get();
		UGP_OrbitalUnitDropDefinition* StubWorkerDrop = NewObject<UGP_OrbitalUnitDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalUnitDrop_Worker_StubPayload")), RF_Transient);
		StubWorkerDrop->Cost = 25.0f;
		StubWorkerDrop->TransportSlotCost = 1;
		StubWorkerDrop->DeliveryDescentSeconds = 0.25f;
		StubWorkerDrop->PayloadDeployDelaySeconds = 0.0f;
		StubWorkerDrop->UnitDefinition = Units.GetWorkerDefinition();
		StubWorkerDrop->PayloadClass = AGP_OrbitalDropContractWorkerStub::StaticClass();
		UnitDrops.DebugAssignLoadedAuthoredWorker(StubWorkerDrop);

		UGP_OrbitalUnitDropDefinition* StubWalkerDrop = NewObject<UGP_OrbitalUnitDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalUnitDrop_Walker_StubPayload")), RF_Transient);
		StubWalkerDrop->Cost = 50.0f;
		StubWalkerDrop->TransportSlotCost = 2;
		StubWalkerDrop->DeliveryDescentSeconds = 0.25f;
		StubWalkerDrop->PayloadDeployDelaySeconds = 0.0f;
		StubWalkerDrop->UnitDefinition = Units.GetSalvageWalkerDefinition();
		StubWalkerDrop->PayloadClass = AGP_OrbitalDropContractWalkerStub::StaticClass();
		UnitDrops.DebugAssignLoadedAuthoredSalvageWalker(StubWalkerDrop);

		Settings->UnitDropPodClass = AGP_OrbitalDropContractPodStub::StaticClass();
		bool bUsedAuthored = true;
		Expect(UnitDrops.ResolveWorkerPayloadClass() == AGP_OrbitalDropContractWorkerStub::StaticClass(),
			TEXT("A_AuthoredReadyWorkerStubWins"));
		Expect(UnitDrops.ResolveSalvageWalkerPayloadClass() == AGP_OrbitalDropContractWalkerStub::StaticClass(),
			TEXT("B_AuthoredReadyWalkerStubWins"));
		Expect(Settings->ResolveUnitDropPodClass(&bUsedAuthored) == AGP_OrbitalDropContractPodStub::StaticClass()
			&& bUsedAuthored, TEXT("E_ResolvePodStub"));

		// Destroy prior team units so class checks are unambiguous
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}

		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		FGP_UnitDropManifest StubWorkers;
		StubWorkers.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult StubWorkerResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, StubWorkers);
		Expect(StubWorkerResult.bAccepted, TEXT("A_StubWorkerOrderAccept"));
		Expect(IsValid(StubWorkerResult.SpawnedPod.Get())
			&& StubWorkerResult.SpawnedPod->IsA(AGP_OrbitalDropContractPodStub::StaticClass()),
			TEXT("E_StubPodSpawned"));
		LastPodWeak = StubWorkerResult.SpawnedPod;

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 5: // Verify stub Worker spawn + stub Walker order
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("AuthoredLand_OwnerAlive")))
		{
			Finish();
			return;
		}

		int32 StubWorkers = 0;
		int32 NativeOnlyWorkers = 0;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() != ContractTeam)
			{
				continue;
			}
			if (It->IsA(AGP_OrbitalDropContractWorkerStub::StaticClass()))
			{
				++StubWorkers;
			}
			else if (It->GetClass() == AGP_Worker::StaticClass())
			{
				++NativeOnlyWorkers;
			}
		}
		Expect(StubWorkers == 1, TEXT("A_StubWorkerSpawned"));
		Expect(NativeOnlyWorkers == 0, TEXT("A_NoNativeWorkerWhenStubConfigured"));

		if (AGP_MainBase* BaseGround = MainBaseWeak.Get())
		{
			if (USceneComponent* Zone = BaseGround->GetUnitDropZone())
			{
				const float GroundZ = Zone->GetComponentLocation().Z;
				for (TActorIterator<AGP_Worker> It(World); It; ++It)
				{
					if (It->GetTeamId() != ContractTeam
						|| !It->IsA(AGP_OrbitalDropContractWorkerStub::StaticClass()))
					{
						continue;
					}
					const float HalfH = GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(It->GetClass());
					Expect(FMath::IsNearlyEqual(It->GetActorLocation().Z - HalfH, GroundZ, 8.0f),
						TEXT("Ground_StubWorkerCapsuleBottom"));
				}
			}
		}

		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		FGP_UnitDropManifest StubWalker;
		StubWalker.SalvageWalkerCount = 1;
		GPUnitDropAuthority::FEvalResult StubWalkerResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, StubWalker);
		Expect(StubWalkerResult.bAccepted, TEXT("B_StubWalkerOrderAccept"));
		Expect(IsValid(StubWalkerResult.SpawnedPod.Get())
			&& StubWalkerResult.SpawnedPod->IsA(AGP_OrbitalDropContractPodStub::StaticClass()),
			TEXT("E_StubPodForWalker"));

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 6: // Verify stub Walker + restore soft empty fallback semantics still hold
	{
		int32 StubWalkers = 0;
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam
				&& It->IsA(AGP_OrbitalDropContractWalkerStub::StaticClass()))
			{
				++StubWalkers;
			}
		}
		Expect(StubWalkers == 1, TEXT("B_StubWalkerSpawned"));

		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugClearAuthoredUnitDropOverrides();
		UnitDrops.DebugBeginContractIsolation();
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->UnitDropPodClass.Reset();
		}
		Expect(UnitDrops.ResolveWorkerPayloadClass() == AGP_Worker::StaticClass()
			&& UnitDrops.ResolveSalvageWalkerPayloadClass() == AGP_SalvageWalker::StaticClass(),
			TEXT("F_FallbackNativeAfterClear"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7: // Deploy delay: no payload before delay
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("Deploy_ActorsAlive")))
		{
			Finish();
			return;
		}

		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->UnitDropDescentDurationSeconds = 0.20f;
			Settings->UnitDropPayloadDeployDelaySeconds = 0.40f;
			UGP_OrbitalUnitDropCatalog::Get().OverrideDeliveryTiming(
				Settings->UnitDropDescentDurationSeconds,
				Settings->UnitDropPayloadDeployDelaySeconds);
			Settings->UnitDropCleanupDelaySeconds = 0.10f;
			Settings->UnitDropPodClass.Reset();
		}

		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 100.0f);
		FGP_UnitDropManifest One;
		One.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult Result =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, One);
		Expect(Result.bAccepted, TEXT("Deploy_OrderAccept"));
		Expect(IsValid(Result.SpawnedPod.Get()), TEXT("Deploy_PodPresent"));
		LastPodWeak = Result.SpawnedPod;
		Expect(Result.SpawnedPod->GetPhase() == EGP_DropPodPhase::Descending
			|| Result.SpawnedPod->GetPhase() == EGP_DropPodPhase::Deploying,
			TEXT("Deploy_PhaseEarly"));

		++StageIndex;
		ScheduleNext(0.28f); // past impact (~0.20), still before deploy (~0.60)
		break;
	}
	case 8: // Still deploying — no payload yet
	{
		AGP_DropPod* Pod = LastPodWeak.Get();
		Expect(IsValid(Pod), TEXT("Deploy_PodStillAlive"));
		if (IsValid(Pod))
		{
			Expect(Pod->GetPhase() == EGP_DropPodPhase::Deploying, TEXT("Deploy_PhaseDeploying"));
		}

		int32 Workers = 0;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				++Workers;
			}
		}
		Expect(Workers == 0, TEXT("Deploy_NoPayloadBeforeDelay"));

		++StageIndex;
		ScheduleNext(0.35f); // past deploy delay
		break;
	}
	case 9: // Payload after delay + cleanup after
	{
		AGP_DropPod* Pod = LastPodWeak.Get();
		int32 Workers = 0;
		AGP_Worker* Spawned = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				++Workers;
				Spawned = *It;
			}
		}
		Expect(Workers == 1, TEXT("Deploy_PayloadAfterDelay"));
		Expect(IsValid(Pod) && Pod->GetPhase() == EGP_DropPodPhase::PayloadDeployed,
			TEXT("Deploy_PhasePayloadDeployed"));

		if (IsValid(Spawned) && IsValid(MainBaseWeak.Get()) && IsValid(MainBaseWeak->GetUnitDropZone()))
		{
			const float GroundZ = MainBaseWeak->GetUnitDropZone()->GetComponentLocation().Z;
			const float HalfH = GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(Spawned->GetClass());
			Expect(FMath::IsNearlyEqual(Spawned->GetActorLocation().Z - HalfH, GroundZ, 8.0f),
				TEXT("Deploy_GroundAfterDelay"));
		}

		// Exactly-once: second AuthorityBegin cannot be called externally; re-request would spend.
		// Verify single worker only.
		Expect(Workers == 1, TEXT("Deploy_ExactlyOncePayload"));

		++StageIndex;
		ScheduleNext(0.20f); // cleanup delay 0.10 + margin
		break;
	}
	case 10: // Cleanup after payload phase
	{
		Expect(!LastPodWeak.IsValid() || !IsValid(LastPodWeak.Get()), TEXT("Deploy_CleanupAfterPayload"));
		int32 Pods = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			++Pods;
		}
		Expect(Pods == 0, TEXT("Deploy_NoPodsRemain"));

		// Zero-delay already exercised in cases 1–6 (UnitDropPayloadDeployDelaySeconds=0).
		Expect(true, TEXT("Deploy_ZeroDelayPathCoveredInCoreFlow"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11: // Native bootstrap remains usable; top-level authored pending rejects spend
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		Expect(UnitDrops.TryGetExisting() == &UnitDrops, TEXT("Nested_CatalogExisting"));
		Expect(IsValid(UnitDrops.GetNativeWorkerDrop())
			&& UnitDrops.GetWorkerDrop() == UnitDrops.GetNativeWorkerDrop()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& FMath::IsNearlyEqual(UnitDrops.GetNativeWorkerDrop()->Cost, 25.0f)
			&& UnitDrops.GetNativeWorkerDrop()->TransportSlotCost == 1
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 25.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 1
			&& IsValid(UnitDrops.GetNativeSalvageWalkerDrop())
			&& UnitDrops.GetSalvageWalkerDrop() == UnitDrops.GetNativeSalvageWalkerDrop()
			&& !UnitDrops.IsSalvageWalkerDropDefinitionPending()
			&& FMath::IsNearlyEqual(UnitDrops.GetNativeSalvageWalkerDrop()->Cost, 50.0f)
			&& UnitDrops.GetNativeSalvageWalkerDrop()->TransportSlotCost == 2
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 50.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 2,
			TEXT("Nested_NativeWorkerWalkerBootstrapUsable"));

		const UClass* SettingsClass = UGP_OrbitalDeliverySettings::StaticClass();
		Expect(SettingsClass != nullptr
			&& SettingsClass->FindPropertyByName(TEXT("WorkerTransportSlotCost")) == nullptr
			&& SettingsClass->FindPropertyByName(TEXT("SalvageWalkerTransportSlotCost")) == nullptr
			&& SettingsClass->FindPropertyByName(TEXT("WorkerOrbitalDropCost")) == nullptr
			&& SettingsClass->FindPropertyByName(TEXT("SalvageWalkerOrbitalDropCost")) == nullptr
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 25.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 1
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 50.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 2,
			TEXT("Numeric_RemovedSettingsCannotAffectCatalog"));

		UGP_OrbitalUnitDropDefinition* AuthoredWalkerDropDef = NewObject<UGP_OrbitalUnitDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalUnitDrop_SalvageWalker_Numeric")), RF_Transient);
		AuthoredWalkerDropDef->Cost = 41.0f;
		AuthoredWalkerDropDef->TransportSlotCost = 4;
		AuthoredWalkerDropDef->UnitDefinition = UGP_UnitDefinitionCatalog::Get().GetSalvageWalkerDefinition();
		AuthoredWalkerDropDef->PayloadClass = AGP_SalvageWalker::StaticClass();
		UnitDrops.DebugAssignLoadedAuthoredSalvageWalker(AuthoredWalkerDropDef);
		Expect(UnitDrops.GetSalvageWalkerDrop() == AuthoredWalkerDropDef
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 41.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 4
			&& FMath::IsNearlyEqual(UnitDrops.GetNativeSalvageWalkerDrop()->Cost, 50.0f)
			&& UnitDrops.GetNativeSalvageWalkerDrop()->TransportSlotCost == 2,
			TEXT("Numeric_AuthoredReadyWalkerWins"));
		UnitDrops.DebugAssignLoadedAuthoredSalvageWalker(nullptr);
		Expect(UnitDrops.GetSalvageWalkerDrop() == UnitDrops.GetNativeSalvageWalkerDrop()
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 50.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 2,
			TEXT("Numeric_WalkerRestoredNativeAfterAuthored"));

		AuthoredWorkerUnitDef = NewObject<UGP_UnitDefinition>(
			this, FName(TEXT("DA_GP_Unit_Worker_AuthoredNested")), RF_Transient);
		AuthoredWorkerUnitDef->DisplayName = NSLOCTEXT("GPOrbitalUnitDrop", "AuthoredWorker", "Authored Worker");
		AuthoredWorkerUnitDef->MaxHealth = 777.0f;
		AuthoredWorkerUnitDef->InitialHealth = 777.0f;
		AuthoredWorkerUnitDef->CargoCapacity = 50.0f;

		AuthoredWorkerDropDef = NewObject<UGP_OrbitalUnitDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalUnitDrop_Worker_Nested")), RF_Transient);
		AuthoredWorkerDropDef->Cost = 17.0f;
		AuthoredWorkerDropDef->TransportSlotCost = 3;
		AuthoredWorkerDropDef->DeliveryDescentSeconds = 0.20f;
		AuthoredWorkerDropDef->PayloadDeployDelaySeconds = 0.0f;
		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		AuthoredWorkerDropDef->PayloadClass = AGP_OrbitalDropContractWorkerStub::StaticClass();

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->UnitDropDescentDurationSeconds = 0.20f;
			Settings->UnitDropPayloadDeployDelaySeconds = 0.0f;
			UnitDrops.OverrideDeliveryTiming(0.20f, 0.0f);
		}

		UnitDrops.DebugForceUnresolvedAuthoredWorkerLoad(AuthoredWorkerDropDef, true);
		Expect(UnitDrops.DebugDidRequestAsyncAuthoredWorkerLoad()
			&& UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == nullptr,
			TEXT("Nested_TopLevelPending"));

		if (Expect(IsValid(OwnerPS), TEXT("Nested_OwnerAlive")))
		{
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			FGP_UnitDropManifest OneWorker;
			OneWorker.WorkerCount = 1;
			const GPUnitDropAuthority::FEvalResult Pending =
				GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, OneWorker);
			Expect(!Pending.bAccepted
				&& Pending.RejectReason == EGP_UnitDropRejectReason::DefinitionNotReady
				&& !Pending.SpawnedPod.IsValid()
				&& FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore, 0.05f),
				TEXT("Nested_TopLevelPendingNoSpend"));
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12: // Top-level loaded, UnitDefinition unresolved → Pending, no spend
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugForceUnresolvedNestedWorkerUnitDefinitionLoad(
			AuthoredWorkerDropDef,
			AuthoredWorkerUnitDef,
			true);
		Expect(UnitDrops.DebugDidRequestAsyncNestedUnitDefinitionLoad()
			&& UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() != AuthoredWorkerDropDef
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 17.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 3
			&& FMath::IsNearlyEqual(UnitDrops.GetNativeWorkerDrop()->Cost, 25.0f)
			&& UnitDrops.GetNativeWorkerDrop()->TransportSlotCost == 1,
			TEXT("Nested_UnitDefinitionPending"));

		if (Expect(IsValid(OwnerPS), TEXT("Nested_UnitDefOwnerAlive")))
		{
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			FGP_UnitDropManifest OneWorker;
			OneWorker.WorkerCount = 1;
			const GPUnitDropAuthority::FEvalResult Pending =
				GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, OneWorker);
			Expect(!Pending.bAccepted
				&& Pending.RejectReason == EGP_UnitDropRejectReason::DefinitionNotReady
				&& FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore, 0.05f),
				TEXT("Nested_UnitDefinitionPendingNoSpend"));
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 13: // UnitDefinition loaded, PayloadClass unresolved → Pending, no spend
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugCompletePendingNestedWorkerUnitDefinitionLoad();
		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		UnitDrops.DebugForceUnresolvedNestedWorkerPayloadClassLoad(
			AuthoredWorkerDropDef,
			AGP_OrbitalDropContractWorkerStub::StaticClass(),
			true);
		Expect(UnitDrops.DebugDidRequestAsyncNestedPayloadClassLoad()
			&& UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() != AuthoredWorkerDropDef
			&& AuthoredWorkerDropDef->ResolveLoadedUnitDefinition() == AuthoredWorkerUnitDef
			&& UnitDrops.ResolveWorkerPayloadClass() == nullptr,
			TEXT("Nested_PayloadClassPending"));

		if (Expect(IsValid(OwnerPS), TEXT("Nested_PayloadOwnerAlive")))
		{
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			FGP_UnitDropManifest OneWorker;
			OneWorker.WorkerCount = 1;
			const GPUnitDropAuthority::FEvalResult Pending =
				GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, OneWorker);
			Expect(!Pending.bAccepted
				&& Pending.RejectReason == EGP_UnitDropRejectReason::DefinitionNotReady
				&& FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore, 0.05f),
				TEXT("Nested_PayloadClassPendingNoSpend"));
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 14: // Nested deps complete → authored Ready, purchase accepted
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugCompletePendingNestedWorkerPayloadClassLoad();
		Expect(!UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == AuthoredWorkerDropDef
			&& AuthoredWorkerDropDef->ResolveLoadedUnitDefinition() == AuthoredWorkerUnitDef
			&& UnitDrops.ResolveWorkerPayloadClass() == AGP_OrbitalDropContractWorkerStub::StaticClass()
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 17.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 3,
			TEXT("Nested_AuthoredReadyUsesPayloadClass"));

		const UClass* SettingsClass = UGP_OrbitalDeliverySettings::StaticClass();
		Expect(SettingsClass->FindPropertyByName(TEXT("WorkerPayloadClass")) == nullptr
			&& SettingsClass->FindPropertyByName(TEXT("SalvageWalkerPayloadClass")) == nullptr
			&& UnitDrops.ResolveWorkerPayloadClass() == AGP_OrbitalDropContractWorkerStub::StaticClass(),
			TEXT("Nested_RemovedSettingsCannotInfluenceCatalog"));

		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				It->Destroy();
			}
		}
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}

		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("Nested_PurchaseActorsAlive")))
		{
			Finish();
			return;
		}

		GPOrbitalUnitDropDebug::GrantOrbital(OwnerPS, 50.0f);
		OrbitalBeforeSpend = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		const GPUnitDropAuthority::FEvalResult Buy =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, OneWorker);
		Expect(Buy.bAccepted
			&& FMath::IsNearlyEqual(Buy.OrbitalCost, 17.0f, 0.05f)
			&& IsValid(Buy.SpawnedPod.Get())
			&& FMath::IsNearlyEqual(
				OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
				OrbitalBeforeSpend - 17.0f,
				0.05f),
			TEXT("Nested_AuthoredPurchaseAccepted"));
		LastPodWeak = Buy.SpawnedPod;

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 15: // Spawned payload uses authored class + UnitDefinition
	{
		int32 StubWorkers = 0;
		int32 NativeWorkers = 0;
		AGP_Worker* Spawned = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (It->GetTeamId() != ContractTeam)
			{
				continue;
			}
			if (It->IsA(AGP_OrbitalDropContractWorkerStub::StaticClass()))
			{
				++StubWorkers;
				Spawned = *It;
			}
			else if (It->GetClass() == AGP_Worker::StaticClass())
			{
				++NativeWorkers;
			}
		}
		Expect(StubWorkers == 1, TEXT("Nested_AuthoredPayloadClassSpawned"));
		Expect(NativeWorkers == 0, TEXT("Nested_DeprecatedSettingsClassDidNotWin"));
		Expect(IsValid(Spawned)
			&& Spawned->ResolveLoadedUnitDefinition() == AuthoredWorkerUnitDef,
			TEXT("Nested_AuthoredUnitDefinitionAppliedToPayload"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 16: // Nested UnitDefinition failure → native fallback, not stuck Pending
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UGP_OrbitalUnitDropDefinition* NativeWorker = UnitDrops.GetNativeWorkerDrop();
		UnitDrops.DebugForceUnresolvedNestedWorkerUnitDefinitionLoad(
			AuthoredWorkerDropDef,
			AuthoredWorkerUnitDef,
			true);
		Expect(UnitDrops.IsWorkerDropDefinitionPending(), TEXT("Nested_FailUnitDefStartsPending"));
		UnitDrops.DebugForceNestedWorkerUnitDefinitionLoadFailure();
		Expect(UnitDrops.DebugConsumeNestedUnitDefinitionLoadFailedLog()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == NativeWorker,
			TEXT("Nested_FailUnitDefNativeFallback"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 17: // Nested PayloadClass failure / invalid subclass → native fallback
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UGP_OrbitalUnitDropDefinition* NativeWorker = UnitDrops.GetNativeWorkerDrop();
		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		AuthoredWorkerDropDef->PayloadClass = AGP_OrbitalDropContractWorkerStub::StaticClass();
		UnitDrops.DebugForceUnresolvedNestedWorkerPayloadClassLoad(
			AuthoredWorkerDropDef,
			AGP_OrbitalDropContractWorkerStub::StaticClass(),
			true);
		Expect(UnitDrops.IsWorkerDropDefinitionPending(), TEXT("Nested_FailPayloadStartsPending"));
		UnitDrops.DebugForceNestedWorkerPayloadClassLoadFailure();
		Expect(UnitDrops.DebugConsumeNestedPayloadClassLoadFailedLog()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == NativeWorker,
			TEXT("Nested_FailPayloadNativeFallback"));

		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		AuthoredWorkerDropDef->PayloadClass = AGP_SalvageWalker::StaticClass();
		UnitDrops.DebugAssignLoadedAuthoredWorker(AuthoredWorkerDropDef);
		Expect(UnitDrops.DebugConsumeNestedPayloadClassLoadFailedLog()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == NativeWorker
			&& UnitDrops.ResolveWorkerPayloadClass() == AGP_Worker::StaticClass(),
			TEXT("Nested_InvalidPayloadSubclassNativeFallback"));

		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		AuthoredWorkerDropDef->PayloadClass.Reset();
		UnitDrops.DebugAssignLoadedAuthoredWorker(AuthoredWorkerDropDef);
		Expect(UnitDrops.DebugConsumeNullPayloadClassLog()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == NativeWorker
			&& UnitDrops.ResolveWorkerPayloadClass() == AGP_Worker::StaticClass(),
			TEXT("Nested_NullPayloadClassNativeFallback"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 18: // Teardown while nested pending: handles cancelled, callback safe, native usable
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		AuthoredWorkerDropDef->UnitDefinition = AuthoredWorkerUnitDef;
		AuthoredWorkerDropDef->PayloadClass = AGP_OrbitalDropContractWorkerStub::StaticClass();
		UnitDrops.DebugForceUnresolvedNestedWorkerUnitDefinitionLoad(
			AuthoredWorkerDropDef,
			AuthoredWorkerUnitDef,
			true);
		Expect(UnitDrops.IsWorkerDropDefinitionPending()
			&& UGP_OrbitalUnitDropCatalog::TryGetExisting() == &UnitDrops,
			TEXT("Nested_TeardownStartsPending"));

		UGP_OrbitalUnitDropCatalog::ShutdownCatalog();
		Expect(UGP_OrbitalUnitDropCatalog::TryGetExisting() == nullptr, TEXT("Nested_TeardownExistingNull"));

		UGP_OrbitalUnitDropCatalog& Recreated = UGP_OrbitalUnitDropCatalog::Get();
		Recreated.DebugCompletePendingNestedWorkerUnitDefinitionLoad();
		Recreated.DebugClearAuthoredUnitDropOverrides();
		Expect(!Recreated.IsWorkerDropDefinitionPending()
			&& Recreated.GetWorkerDrop() == Recreated.GetNativeWorkerDrop()
			&& Recreated.GetSalvageWalkerDrop() == Recreated.GetNativeSalvageWalkerDrop(),
			TEXT("Nested_TeardownNativeUsableNoResurrectionFatal"));

		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_OrbitalUnitDropContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_OrbitalUnitDropContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_OrbitalUnitDropContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_OrbitalUnitDropContractTestRunner::AdvanceStage() {}
bool UGP_OrbitalUnitDropContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_OrbitalUnitDropContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_OrbitalUnitDropContractTestRunner::Finish() { bFinished = true; }
void UGP_OrbitalUnitDropContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_OrbitalUnitDropContractTestRunner::UnbindWorldCleanup() {}
void UGP_OrbitalUnitDropContractTestRunner::CleanupActors() {}
void UGP_OrbitalUnitDropContractTestRunner::RestoreSettings() {}

#endif
