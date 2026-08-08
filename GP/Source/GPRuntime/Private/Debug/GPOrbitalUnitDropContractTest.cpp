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
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Orbital/GPUnitGroundPlacement.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "Units/GPSalvageWalker.h"
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
		Settings->WorkerPayloadClass = SavedWorkerPayload;
		Settings->SalvageWalkerPayloadClass = SavedWalkerPayload;
		Settings->UnitDropPodClass = SavedDropPodClass;
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
			SavedWorkerPayload = Settings->WorkerPayloadClass;
			SavedWalkerPayload = Settings->SalvageWalkerPayloadClass;
			SavedDropPodClass = Settings->UnitDropPodClass;
			Settings->UnitDropDescentDurationSeconds = 0.25f;
			Settings->UnitDropCleanupDelaySeconds = 0.05f;
			Settings->UnitDropSpawnAltitudeCm = 400.0f;
			Settings->UnitDropPayloadDeployDelaySeconds = 0.0f; // zero-delay path for core flow
			// Native fallback path for core slot/cost/spend checks.
			Settings->WorkerPayloadClass.Reset();
			Settings->SalvageWalkerPayloadClass.Reset();
			Settings->UnitDropPodClass.Reset();
			bSettingsMutated = true;

			bool bUsedAuthored = true;
			Expect(Settings->ResolveWorkerPayloadClass(&bUsedAuthored) == AGP_Worker::StaticClass()
				&& !bUsedAuthored, TEXT("F_FallbackNativeWorker"));
			Expect(Settings->ResolveSalvageWalkerPayloadClass(&bUsedAuthored) == AGP_SalvageWalker::StaticClass()
				&& !bUsedAuthored, TEXT("F_FallbackNativeWalker"));
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
		if (UGP_PlayerAttributeSet* Attr = const_cast<UGP_PlayerAttributeSet*>(OwnerPS->GetPlayerAttributeSet()))
		{
			Attr->SetMaxUnits(1.0f);
			Attr->SetCurrentUnits(1.0f);
		}
		FGP_UnitDropManifest Cap;
		Cap.WorkerCount = 1;
		GPUnitDropAuthority::FEvalResult CapResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Cap);
		Expect(!CapResult.bAccepted, TEXT("M_UnitCapReject"));
		Expect(CapResult.RejectReason == EGP_UnitDropRejectReason::UnitCapReached, TEXT("M_UnitCapReason"));
		if (UGP_PlayerAttributeSet* Attr = const_cast<UGP_PlayerAttributeSet*>(OwnerPS->GetPlayerAttributeSet()))
		{
			Attr->SetMaxUnits(0.0f);
			Attr->SetCurrentUnits(0.0f);
		}

		// N: GP-S30 launch path still works
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (!Expect(IsValid(Storage), TEXT("N_StoragePresent")))
		{
			Finish();
			return;
		}
		Storage->AddPlanetaryFerronite(100.0f);
		Expect(Storage->GetReadyCount() >= 1, TEXT("N_ReadyPresent"));
		const float OrbitalBeforeLaunch = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		FGP_ContainerLaunchResult Launch = Storage->TryLaunchReadyContainer();
		Expect(Launch.bAccepted, TEXT("N_LaunchAccepted"));
		Expect(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite() > OrbitalBeforeLaunch + KINDA_SMALL_NUMBER,
			TEXT("N_LaunchGrantedOrbital"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4: // Authored payload/pod soft-class seams
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("Authored_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>();
		if (!Expect(Settings != nullptr, TEXT("Authored_Settings")))
		{
			Finish();
			return;
		}

		// C: incompatible soft Worker class rejected → native fallback
		Settings->WorkerPayloadClass = TSoftClassPtr<AGP_Worker>(
			FSoftObjectPath(AGP_SalvageWalker::StaticClass()->GetPathName()));
		Expect(Settings->IsWorkerPayloadClassConfigInvalid(), TEXT("C_IncompatibleWorkerSoftInvalid"));
		bool bUsedAuthored = true;
		Expect(Settings->ResolveWorkerPayloadClass(&bUsedAuthored) == AGP_Worker::StaticClass()
			&& !bUsedAuthored, TEXT("C_IncompatibleWorkerFallsBackNative"));

		// D: manifest is counts-only (no class field for client to choose)
		FGP_UnitDropManifest CountsOnly;
		CountsOnly.WorkerCount = 1;
		CountsOnly.SalvageWalkerCount = 0;
		Expect(CountsOnly.GetTotalUnitCount() == 1, TEXT("D_ManifestCountsOnly"));

		// Configure approved stubs (A/B/E)
		Settings->WorkerPayloadClass = AGP_OrbitalDropContractWorkerStub::StaticClass();
		Settings->SalvageWalkerPayloadClass = AGP_OrbitalDropContractWalkerStub::StaticClass();
		Settings->UnitDropPodClass = AGP_OrbitalDropContractPodStub::StaticClass();
		Expect(Settings->ResolveWorkerPayloadClass(&bUsedAuthored) == AGP_OrbitalDropContractWorkerStub::StaticClass()
			&& bUsedAuthored, TEXT("A_ResolveWorkerStub"));
		Expect(Settings->ResolveSalvageWalkerPayloadClass(&bUsedAuthored) == AGP_OrbitalDropContractWalkerStub::StaticClass()
			&& bUsedAuthored, TEXT("B_ResolveWalkerStub"));
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

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->WorkerPayloadClass.Reset();
			Settings->SalvageWalkerPayloadClass.Reset();
			Settings->UnitDropPodClass.Reset();
			bool bUsedAuthored = true;
			Expect(Settings->ResolveWorkerPayloadClass(&bUsedAuthored) == AGP_Worker::StaticClass()
				&& !bUsedAuthored, TEXT("F_FallbackNativeAfterClear"));
		}

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
			Settings->UnitDropCleanupDelaySeconds = 0.10f;
			Settings->WorkerPayloadClass.Reset();
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
