// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalBuildingDropContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPBuildingGroundPlacement.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPOrbitalBuildingDrop, Log, All);

namespace GPOrbitalBuildingDropDebug
{
	static TWeakObjectPtr<UGP_OrbitalBuildingDropContractTestRunner> GActiveRunner;

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

	static void RunOrbitalBuildingDropContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPOrbitalBuildingDrop, Warning, TEXT("gp.Building.RunOrbitalBuildingDropContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPOrbitalBuildingDrop, Warning, TEXT("gp.Building.RunOrbitalBuildingDropContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("OrbitalBuildingDropContract"), TEXT("OrbitalBuildingDrop"), Token))
		{
			return;
		}

		UGP_OrbitalBuildingDropContractTestRunner* Runner =
			NewObject<UGP_OrbitalBuildingDropContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GOrbitalBuildingDropContract(
		TEXT("gp.Building.RunOrbitalBuildingDropContractTest"),
		TEXT("GP-S32R orbital building purchase/READY/deploy contract (A–O)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunOrbitalBuildingDropContractTest));
}

void UGP_OrbitalBuildingDropContractTestRunner::BeginDestroy()
{
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_OrbitalBuildingDropContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_OrbitalBuildingDropContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		bCancelled = true;
		CancelReason = FName(TEXT("WorldCleanup"));
		Finish();
	}
}

void UGP_OrbitalBuildingDropContractTestRunner::CleanupCatalogIfExists()
{
	if (UGP_BuildingDropCatalog* Catalog = UGP_BuildingDropCatalog::TryGetExisting())
	{
		Catalog->DebugEndContractIsolation();
	}
}

void UGP_OrbitalBuildingDropContractTestRunner::RestoreSettings()
{
	if (bSettingsMutated)
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->BuildingDropCleanupDelaySeconds = SavedBuildingCleanup;
			Settings->BuildingDropSpawnAltitudeCm = SavedBuildingAltitude;
			if (UGP_BuildingDropCatalog* Catalog = UGP_BuildingDropCatalog::TryGetExisting())
			{
				Catalog->OverrideDeliveryTiming(
					UGP_BuildingDropCatalog::NativeDeliveryDescentSeconds,
					UGP_BuildingDropCatalog::NativePayloadDeployDelaySeconds);
			}
			Settings->BuildingOrbitalPurchaseCost = SavedBuildingPurchaseCost;
			Settings->BuildingMaxDeployRadiusFromMainBaseCm = SavedBuildingMaxRadius;
			Settings->BuildingPayloadClass = SavedBuildingPayload;
		}
		bSettingsMutated = false;
	}
	CleanupCatalogIfExists();
}

void UGP_OrbitalBuildingDropContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_PlayerState> It(World); It; ++It)
		{
			It->Destroy();
		}
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
		}
	}
	MainBaseWeak.Reset();
	OwnerPSWeak.Reset();
	LastPodWeak.Reset();
}

void UGP_OrbitalBuildingDropContractTestRunner::Finish()
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
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPOrbitalBuildingDrop, Log,
		TEXT("gp.Building.RunOrbitalBuildingDropContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPOrbitalBuildingDropDebug::GActiveRunner.Reset();
}

void UGP_OrbitalBuildingDropContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPOrbitalBuildingDrop, Error,
		TEXT("gp.Building.RunOrbitalBuildingDropContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_OrbitalBuildingDropContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPOrbitalBuildingDrop, Error,
			TEXT("gp.Building.RunOrbitalBuildingDropContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPOrbitalBuildingDrop, Log,
		TEXT("gp.Building.RunOrbitalBuildingDropContractTest PASS: %s"), Label);
	return true;
}

void UGP_OrbitalBuildingDropContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_OrbitalBuildingDropContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_OrbitalBuildingDropContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_OrbitalBuildingDropContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPOrbitalBuildingDrop, Log, TEXT("gp.Building.RunOrbitalBuildingDropContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_OrbitalBuildingDropContractTestRunner::AdvanceStage()
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

	constexpr int32 ContractTeam = 82;

	switch (StageIndex)
	{
	case 0: // Setup + A/B rejects
	{
		UGP_BuildingDropCatalog::Get().DebugBeginContractIsolation();
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			SavedBuildingPurchaseCost = Settings->BuildingOrbitalPurchaseCost;
			SavedBuildingMaxRadius = Settings->BuildingMaxDeployRadiusFromMainBaseCm;
			SavedBuildingPayload = Settings->BuildingPayloadClass;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			Settings->BuildingPayloadClass.Reset();
			bSettingsMutated = true;

			UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
			const UClass* SettingsClass = UGP_OrbitalDeliverySettings::StaticClass();
			Expect(SettingsClass->FindPropertyByName(TEXT("BuildingDropDescentDurationSeconds")) == nullptr
				&& SettingsClass->FindPropertyByName(TEXT("BuildingDropPayloadDeployDelaySeconds")) == nullptr
				&& SettingsClass->FindPropertyByName(TEXT("UnitDropDescentDurationSeconds")) == nullptr
				&& SettingsClass->FindPropertyByName(TEXT("UnitDropPayloadDeployDelaySeconds")) == nullptr,
				TEXT("Timing_RemovedSettingsFieldsAbsent"));
			float NativeDescent = 0.0f;
			float NativeDeploy = 0.0f;
			Buildings.ResolveDeliveryTiming(Buildings.GetLegacyLogisticsHubDrop(), NativeDescent, NativeDeploy);
			Expect(FMath::IsNearlyEqual(NativeDescent, UGP_BuildingDropCatalog::NativeDeliveryDescentSeconds)
				&& FMath::IsNearlyEqual(NativeDeploy, UGP_BuildingDropCatalog::NativePayloadDeployDelaySeconds),
				TEXT("Timing_NativeBuildingDrop"));
			Buildings.OverrideDeliveryTiming(0.25f, 0.0f);

			bool bUsedAuthored = true;
			Expect(Settings->ResolveBuildingPayloadClass(&bUsedAuthored) == AGP_LogisticsHub::StaticClass()
				&& !bUsedAuthored, TEXT("M_FallbackNativeHub"));
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-48000.0f, 2000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(ContractTeam);
		ValidDeployLocation = Base->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f);

		AGP_PlayerState* OwnerPS = GPOrbitalBuildingDropDebug::SpawnTeamPlayerState(World, GS, ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS), TEXT("SpawnOwnerPS")))
		{
			Finish();
			return;
		}

		GPBuildingDropAuthority::FPurchaseResult InvalidType =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, EGP_OrbitalBuildingType::None);
		Expect(!InvalidType.bAccepted, TEXT("A_InvalidTypeReject"));
		Expect(InvalidType.RejectReason == EGP_BuildingDropRejectReason::InvalidType, TEXT("A_InvalidTypeReason"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 0,
			TEXT("A_NoReadyOnInvalidType"));

		GPBuildingDropAuthority::FPurchaseResult Poor =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub);
		Expect(!Poor.bAccepted, TEXT("B_InsufficientOrbitalReject"));
		Expect(Poor.RejectReason == EGP_BuildingDropRejectReason::InsufficientOrbital, TEXT("B_InsufficientReason"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 0,
			TEXT("B_NoReadyNoSpend"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1: // C purchase success
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("C_OwnerAlive")))
		{
			Finish();
			return;
		}

		GPOrbitalBuildingDropDebug::GrantOrbital(OwnerPS, 100.0f);
		OrbitalBeforePurchase = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();

		GPBuildingDropAuthority::FPurchaseResult Purchase =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub);
		Expect(Purchase.bAccepted, TEXT("C_PurchaseAccept"));
		Expect(Purchase.ReadyAfter == 1, TEXT("C_ReadyIncrement"));
		Expect(FMath::IsNearlyEqual(Purchase.OrbitalCost, 100.0f, 0.05f), TEXT("C_Cost"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBeforePurchase - 100.0f,
			0.05f), TEXT("C_SpendOnce"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2: // Placement input ownership + D deploy without ready + E radius + F overlap
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("D_ActorsAlive")))
		{
			Finish();
			return;
		}

		// --- Placement input ownership (gates / selection clear / RMB cancel Ready) ---
		if (AGP_PlayerController* PC = Cast<AGP_PlayerController>(World->GetFirstPlayerController()))
		{
			AGP_PlayerState* PCPS = PC->GetPlayerState<AGP_PlayerState>();
			UGP_SelectionComponent* Selection = PC->GetSelectionComponent();
			UGP_OrbitalBuildingInventoryComponent* PCInv =
				PCPS != nullptr ? PCPS->GetOrbitalBuildingInventoryComponent() : nullptr;
			if (Expect(IsValid(PCPS) && Selection != nullptr && PCInv != nullptr, TEXT("P_PCSelectionInventory")))
			{
				if (PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) < 1)
				{
					PCInv->AuthorityAddReady(EGP_OrbitalBuildingType::LogisticsHub, 1);
				}

				FActorSpawnParameters WorkerParams;
				WorkerParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				WorkerParams.ObjectFlags |= RF_Transient;
				AGP_Worker* SelectedWorker = World->SpawnActor<AGP_Worker>(
					AGP_Worker::StaticClass(),
					Base->GetActorLocation() + FVector(0.0f, 600.0f, 100.0f),
					FRotator::ZeroRotator,
					WorkerParams);
				if (Expect(IsValid(SelectedWorker), TEXT("P_SpawnSelectedWorker")))
				{
					SelectedWorker->SetTeamId(ContractTeam);
					Selection->ReplaceSelectionWithUnit(SelectedWorker);
					Expect(Selection->HasSelection(), TEXT("P_SelectionBeforeEnter"));

					PC->EnterBuildingPlacementMode(EGP_OrbitalBuildingType::LogisticsHub);
					Expect(PC->IsBuildingPlacementActive(), TEXT("P_PlacementActive"));
					Expect(!Selection->HasSelection(), TEXT("P_EnterClearsSelection"));
					Expect(PC->IsBuildingPlacementCommandInputBlocked(), TEXT("P_CommandBlockedWhileActive"));
					Expect(PC->IsBuildingPlacementSelectionInputBlocked(), TEXT("P_SelectionBlockedWhileActive"));

					const int32 ReadyBeforeCancel =
						PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub);
					Expect(PC->ConsumeBuildingPlacementCommandInput(), TEXT("P_ConsumeCommandCancels"));
					Expect(!PC->IsBuildingPlacementActive(), TEXT("P_CancelledInactive"));
					Expect(PC->IsBuildingPlacementCommandInputBlocked(), TEXT("P_CommandSuppressedUntilRMBRelease"));
					Expect(PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == ReadyBeforeCancel,
						TEXT("P_CancelPreservesReady"));

					PC->UpdateBuildingPlacementInputEdgesForContract(/*bLMBDown=*/false, /*bRMBDown=*/false);
					Expect(!PC->IsBuildingPlacementCommandInputBlocked(), TEXT("P_CommandUnblockedAfterRMBRelease"));
					Expect(!PC->IsBuildingPlacementSelectionInputBlocked(), TEXT("P_SelectionUnblockedAfterExit"));

					// RMB edge cancel path (Tick/contract edges) also preserves READY.
					if (PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) < 1)
					{
						PCInv->AuthorityAddReady(EGP_OrbitalBuildingType::LogisticsHub, 1);
					}
					PC->EnterBuildingPlacementMode(EGP_OrbitalBuildingType::LogisticsHub);
					const int32 ReadyBeforeEdgeCancel =
						PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub);
					PC->UpdateBuildingPlacementInputEdgesForContract(/*bLMBDown=*/false, /*bRMBDown=*/false);
					PC->UpdateBuildingPlacementInputEdgesForContract(/*bLMBDown=*/false, /*bRMBDown=*/true);
					Expect(!PC->IsBuildingPlacementActive(), TEXT("P_EdgeRMBCancels"));
					Expect(PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == ReadyBeforeEdgeCancel,
						TEXT("P_EdgeRMBCancelPreservesReady"));
					PC->UpdateBuildingPlacementInputEdgesForContract(/*bLMBDown=*/false, /*bRMBDown=*/false);

					SelectedWorker->Destroy();
				}

				// Leave PC inventory at 0 so OwnerPS Ready=1 remains the deploy SoT for later stages.
				while (PCInv->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) > 0)
				{
					PCInv->AuthorityTryConsumeReady(EGP_OrbitalBuildingType::LogisticsHub, 1);
				}
				if (PC->IsBuildingPlacementActive())
				{
					PC->CancelBuildingPlacement();
				}
				PC->UpdateBuildingPlacementInputEdgesForContract(false, false);
			}
		}
		else
		{
			Expect(false, TEXT("P_MissingLocalPC"));
		}

		// Temporarily zero ready
		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		Expect(Inventory != nullptr && Inventory->AuthorityTryConsumeReady(EGP_OrbitalBuildingType::LogisticsHub, 1),
			TEXT("D_ConsumeForNoReadyTest"));
		Expect(Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 0, TEXT("D_ReadyZero"));

		const FTransform ValidTransform(FRotator::ZeroRotator, ValidDeployLocation);
		GPBuildingDropAuthority::FDeployResult NoReady =
			GPBuildingDropAuthority::AuthorityDeployBuilding(World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub, ValidTransform);
		Expect(!NoReady.bAccepted, TEXT("D_NoReadyReject"));
		Expect(NoReady.RejectReason == EGP_BuildingDropRejectReason::NoReadyInventory, TEXT("D_NoReadyReason"));

		Inventory->AuthorityAddReady(EGP_OrbitalBuildingType::LogisticsHub, 1);

		const FVector FarLoc = Base->GetActorLocation() + FVector(99999.0f, 0.0f, 0.0f);
		GPBuildingDropAuthority::FDeployResult Far =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(FRotator::ZeroRotator, FarLoc));
		Expect(!Far.bAccepted, TEXT("E_OutOfRadiusReject"));
		Expect(Far.RejectReason == EGP_BuildingDropRejectReason::OutOfDeployRadius, TEXT("E_OutOfRadiusReason"));
		Expect(Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 1, TEXT("E_ReadyPreserved"));

		GPBuildingDropAuthority::FDeployResult Overlap =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(FRotator::ZeroRotator, Base->GetActorLocation()));
		Expect(!Overlap.bAccepted, TEXT("F_OverlapReject"));
		Expect(Overlap.RejectReason == EGP_BuildingDropRejectReason::GridOccupied, TEXT("F_OverlapReason"));
		Expect(Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 1, TEXT("F_ReadyPreserved"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3: // G/H valid deploy — pod + consume ready, no orbital spend
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("G_OwnerAlive")))
		{
			Finish();
			return;
		}

		OrbitalBeforeDeploy = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const FTransform ValidTransform(FRotator::ZeroRotator, ValidDeployLocation);
		GPBuildingDropAuthority::FDeployResult Deploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				ValidTransform);
		Expect(Deploy.bAccepted, TEXT("G_DeployAccept"));
		Expect(Deploy.ReadyAfter == 0, TEXT("G_ReadyConsumed"));
		Expect(IsValid(Deploy.SpawnedPod.Get()), TEXT("G_PodSpawned"));
		if (IsValid(Deploy.SpawnedPod.Get()))
		{
			Expect(Deploy.SpawnedPod->GetPayloadKind() == EGP_DropPodPayloadKind::Building, TEXT("G_BuildingPayloadKind"));
			float ExpectedDescent = 0.0f;
			float ExpectedDeploy = 0.0f;
			UGP_BuildingDropCatalog::Get().ResolveDeliveryTiming(
				UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop(),
				ExpectedDescent,
				ExpectedDeploy);
			Expect(FMath::IsNearlyEqual(Deploy.SpawnedPod->DebugGetDescentDurationSeconds(), ExpectedDescent)
				&& FMath::IsNearlyEqual(Deploy.SpawnedPod->DebugGetPayloadDeployDelaySeconds(), ExpectedDeploy),
				TEXT("Timing_PodUsesResolvedProduct"));
		}
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBeforeDeploy,
			0.05f), TEXT("J_NoOrbitalSpendOnDeploy"));
		LastPodWeak = Deploy.SpawnedPod;

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 4: // H/I building spawn + ground + team
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("H_OwnerAlive")))
		{
			Finish();
			return;
		}

		int32 HubCount = 0;
		AGP_LogisticsHub* Hub = nullptr;
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam)
			{
				++HubCount;
				Hub = *It;
			}
		}
		Expect(HubCount == 1, TEXT("H_HubSpawnCount"));
		if (IsValid(Hub))
		{
			Expect(Hub->GetTeamId() == ContractTeam, TEXT("H_TeamId"));
			const float HalfH = GPBuildingGroundPlacement::GetGroundSpawnOffsetZForBuildingClass(Hub->GetClass());
			const float BottomZ = Hub->GetActorLocation().Z - HalfH;
			Expect(FMath::IsNearlyEqual(BottomZ, ValidDeployLocation.Z, 8.0f), TEXT("I_GroundCapsuleBottom"));
		}

		GPBuildingDropAuthority::FDeployResult Dup =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation + FVector(0.0f, 400.0f, 0.0f)));
		Expect(!Dup.bAccepted, TEXT("K_DuplicateNoReadyReject"));
		Expect(Dup.RejectReason == EGP_BuildingDropRejectReason::NoReadyInventory, TEXT("K_DuplicateReason"));

		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 5: // L missing main base + M/N authored stub + O unit drop regression
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("L_OwnerAlive")))
		{
			Finish();
			return;
		}

		AGP_MainBase* Base = MainBaseWeak.Get();
		if (IsValid(Base))
		{
			Base->Destroy();
			MainBaseWeak.Reset();
		}

		GPOrbitalBuildingDropDebug::GrantOrbital(OwnerPS, 100.0f);
		GPBuildingDropAuthority::FPurchaseResult NoBase =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub);
		Expect(!NoBase.bAccepted, TEXT("L_MissingMainBasePurchaseReject"));
		Expect(NoBase.RejectReason == EGP_BuildingDropRejectReason::MissingMainBase, TEXT("L_MissingMainBaseReason"));

		// Clear hubs from earlier deploy so interim overlap validation does not reject the stub pad.
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-48000.0f, 2000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("L_RespawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(ContractTeam);
		ValidDeployLocation = Base->GetActorLocation() + FVector(1200.0f, 0.0f, 0.0f);

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->BuildingPayloadClass = AGP_OrbitalBuildingDropContractHubStub::StaticClass();
			bool bUsedAuthored = false;
			Expect(Settings->ResolveBuildingPayloadClass(&bUsedAuthored) == AGP_OrbitalBuildingDropContractHubStub::StaticClass()
				&& bUsedAuthored, TEXT("N_ResolveStubHub"));
		}

		OwnerPS->GetOrbitalBuildingInventoryComponent()->AuthorityAddReady(EGP_OrbitalBuildingType::LogisticsHub, 1);
		GPBuildingDropAuthority::FDeployResult StubDeploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation));
		Expect(StubDeploy.bAccepted, TEXT("N_StubDeployAccept"));
		if (IsValid(StubDeploy.SpawnedPod.Get()))
		{
			LastPodWeak = StubDeploy.SpawnedPod;
		}

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 6: // N stub landed + O unit drop still works
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("N_ActorsAlive")))
		{
			Finish();
			return;
		}

		int32 StubHubs = 0;
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == ContractTeam
				&& It->IsA(AGP_OrbitalBuildingDropContractHubStub::StaticClass()))
			{
				++StubHubs;
			}
		}
		Expect(StubHubs >= 1, TEXT("N_StubHubSpawned"));

		if (IsValid(Base->GetUnitDropZone()))
		{
			GPOrbitalBuildingDropDebug::GrantOrbital(OwnerPS, 50.0f);
			FGP_UnitDropManifest Workers;
			Workers.WorkerCount = 1;
			GPUnitDropAuthority::FEvalResult UnitResult =
				GPUnitDropAuthority::AuthorityRequestUnitDrop(World, OwnerPS, Workers);
			Expect(UnitResult.bAccepted, TEXT("O_UnitDropRegressionAccept"));
		}
		else
		{
			Expect(false, TEXT("O_MissingDropZone"));
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7: // Nested BuildingDefinition pending: no spend / no READY
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("Nested_OwnerAlive")))
		{
			Finish();
			return;
		}

		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		UGP_BuildingDefinition* HubBuilding = Catalog.GetLegacyLogisticsHubDrop() != nullptr
			? Catalog.GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition()
			: nullptr;
		AuthoredHubDropDef = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_NestedPending")), RF_Transient);
		AuthoredHubDropDef->Cost = 17.0f;
		AuthoredHubDropDef->BuildingDefinition = HubBuilding;
		AuthoredHubDropDef->DeliveryDescentSeconds = 4.25f;
		AuthoredHubDropDef->PayloadDeployDelaySeconds = 0.75f;

		Catalog.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(AuthoredHubDropDef, HubBuilding, true);
		Expect(Catalog.DebugDidRequestAsyncNestedBuildingLoad()
			&& Catalog.IsDropDefinitionPending(AuthoredHubDropDef)
			&& Catalog.GetLegacyLogisticsHubDrop() != AuthoredHubDropDef,
			TEXT("Q_NestedPendingKeepsAuthoredNotReady"));

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		const int32 ReadyBefore = Inventory != nullptr
			? Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub)
			: -1;
		GPOrbitalBuildingDropDebug::GrantOrbital(OwnerPS, 50.0f);
		OrbitalBeforePurchase = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		GPBuildingDropAuthority::FPurchaseResult PendingBuy =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, AuthoredHubDropDef);
		const float OrbitalAfter = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const int32 ReadyAfter = Inventory != nullptr
			? Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub)
			: -1;
		Expect(!PendingBuy.bAccepted
			&& PendingBuy.RejectReason == EGP_BuildingDropRejectReason::DefinitionNotReady
			&& FMath::IsNearlyEqual(OrbitalAfter, OrbitalBeforePurchase, 0.05f)
			&& ReadyAfter == ReadyBefore,
			TEXT("Q_NestedPendingDefinitionNotReadyNoSpend"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8: // Nested resolve → authored Ready, authored cost, READY +1
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		Catalog.DebugCompletePendingNestedBuildingLoad();
		Expect(!Catalog.IsDropDefinitionPending(AuthoredHubDropDef)
			&& Catalog.GetLegacyLogisticsHubDrop() == AuthoredHubDropDef
			&& FMath::IsNearlyEqual(Catalog.GetPurchaseCost(AuthoredHubDropDef), 17.0f)
			&& AuthoredHubDropDef != nullptr
			&& AuthoredHubDropDef->ResolveLoadedBuildingDefinition() != nullptr,
			TEXT("R_NestedResolveMakesAuthoredReady"));
		float AuthoredDescent = 0.0f;
		float AuthoredDeploy = 0.0f;
		Catalog.ResolveDeliveryTiming(AuthoredHubDropDef, AuthoredDescent, AuthoredDeploy);
		Expect(FMath::IsNearlyEqual(AuthoredDescent, 4.25f)
			&& FMath::IsNearlyEqual(AuthoredDeploy, 0.75f),
			TEXT("Timing_AuthoredBuildingWins"));
		AuthoredHubDropDef->DeliveryDescentSeconds = 0.25f;
		AuthoredHubDropDef->PayloadDeployDelaySeconds = 0.0f;

		if (!Expect(IsValid(OwnerPS), TEXT("R_OwnerAlive")))
		{
			Finish();
			return;
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		const int32 ReadyBefore = Inventory != nullptr
			? Inventory->GetReadyCount(AuthoredHubDropDef)
			: -1;
		OrbitalBeforePurchase = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		GPBuildingDropAuthority::FPurchaseResult Buy =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, AuthoredHubDropDef);
		Expect(Buy.bAccepted
			&& FMath::IsNearlyEqual(Buy.OrbitalCost, 17.0f, 0.05f)
			&& Buy.ReadyAfter == ReadyBefore + 1
			&& FMath::IsNearlyEqual(
				OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
				OrbitalBeforePurchase - 17.0f,
				0.05f),
			TEXT("R_AuthoredCost17ReadyOnce"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9: // Cold-load top-level then nested
	{
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		UGP_BuildingDefinition* HubBuilding = AuthoredHubDropDef != nullptr
			? AuthoredHubDropDef->ResolveLoadedBuildingDefinition()
			: nullptr;
		UGP_OrbitalDropDefinition* ColdDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_Cold")), RF_Transient);
		ColdDrop->Cost = 19.0f;
		ColdDrop->BuildingDefinition = HubBuilding;
		AuthoredHubDropDef = ColdDrop;

		Catalog.DebugForceUnresolvedAuthoredLogisticsHubLoad(ColdDrop, true);
		Expect(Catalog.DebugDidRequestAsyncAuthoredDropLoad()
			&& Catalog.IsDropDefinitionPending(Catalog.GetLegacyLogisticsHubDrop()),
			TEXT("S_ColdTopLevelPending"));

		Catalog.DebugCompletePendingAuthoredLogisticsHubLoad();
		Expect(!Catalog.IsDropDefinitionPending(ColdDrop)
			&& Catalog.GetLegacyLogisticsHubDrop() == ColdDrop
			&& ColdDrop->ResolveLoadedBuildingDefinition() == HubBuilding,
			TEXT("S_ColdTopLevelResolveKeepsBuilding"));

		Catalog.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(ColdDrop, HubBuilding, true);
		Expect(Catalog.DebugDidRequestAsyncNestedBuildingLoad()
			&& Catalog.IsDropDefinitionPending(ColdDrop)
			&& Catalog.GetLegacyLogisticsHubDrop() != ColdDrop,
			TEXT("S_ColdNestedPendingAfterTopLevel"));

		Catalog.DebugCompletePendingNestedBuildingLoad();
		Expect(!Catalog.IsDropDefinitionPending(ColdDrop)
			&& Catalog.GetLegacyLogisticsHubDrop() == ColdDrop
			&& FMath::IsNearlyEqual(Catalog.GetPurchaseCost(ColdDrop), 19.0f)
			&& ColdDrop->ResolveLoadedBuildingDefinition() == HubBuilding,
			TEXT("S_ColdNestedResolveReady"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10: // Null BuildingDefinition → explicit fallback; failed nested load
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		UGP_OrbitalDropDefinition* NativeHub = nullptr;
		{
			Catalog.DebugClearAuthoredBuildingDropOverrides();
			Catalog.DebugBeginContractIsolation();
			NativeHub = Catalog.GetLegacyLogisticsHubDrop();
		}

		UGP_OrbitalDropDefinition* NullBuildingDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_NullBuilding")), RF_Transient);
		NullBuildingDrop->Cost = 21.0f;
		NullBuildingDrop->BuildingDefinition.Reset();
		Catalog.DebugAssignLoadedAuthoredLogisticsHub(NullBuildingDrop);
		Expect(Catalog.DebugConsumeNullBuildingDefinitionLog()
			&& !Catalog.IsDropDefinitionPending(NullBuildingDrop)
			&& Catalog.GetLegacyLogisticsHubDrop() == NativeHub
			&& FMath::IsNearlyEqual(Catalog.GetPurchaseCost(NativeHub), 100.0f),
			TEXT("T_NullBuildingDefinitionNativeFallback"));

		UGP_BuildingDefinition* HubBuilding = NativeHub != nullptr
			? NativeHub->ResolveLoadedBuildingDefinition()
			: nullptr;
		UGP_OrbitalDropDefinition* FailedNestedDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_FailedNested")), RF_Transient);
		FailedNestedDrop->Cost = 23.0f;
		FailedNestedDrop->BuildingDefinition = HubBuilding;
		Catalog.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(FailedNestedDrop, HubBuilding, true);
		Expect(Catalog.IsDropDefinitionPending(FailedNestedDrop), TEXT("T_FailedNestedStartsPending"));
		Catalog.DebugForceNestedBuildingLoadFailure();
		Expect(Catalog.DebugConsumeNestedBuildingLoadFailedLog()
			&& !Catalog.IsDropDefinitionPending(FailedNestedDrop)
			&& Catalog.GetLegacyLogisticsHubDrop() == NativeHub,
			TEXT("T_FailedNestedNativeFallbackNotStuckPending"));

		if (IsValid(OwnerPS))
		{
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			GPBuildingDropAuthority::FPurchaseResult FailedBuy =
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, FailedNestedDrop);
			Expect(!FailedBuy.bAccepted
				&& FailedBuy.RejectReason == EGP_BuildingDropRejectReason::MissingBuildingDefinition
				&& FMath::IsNearlyEqual(
					OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
					OrbitalBefore,
					0.05f),
				TEXT("T_FailedNestedPointerPurchaseNoSpend"));
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11: // DefensiveTurret nested dependency + native bootstrap still immediate
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		Catalog.DebugClearAuthoredBuildingDropOverrides();
		Catalog.DebugBeginContractIsolation();

		UGP_OrbitalDropDefinition* NativeTurret = Catalog.DebugGetCanonicalDefensiveTurretDrop();
		Expect(IsValid(NativeTurret)
			&& NativeTurret->ResolveLoadedBuildingDefinition() != nullptr
			&& !Catalog.IsDropDefinitionPending(NativeTurret),
			TEXT("U_NativeTurretImmediatelyUsable"));

		UGP_BuildingDefinition* TurretBuilding = NativeTurret != nullptr
			? NativeTurret->ResolveLoadedBuildingDefinition()
			: nullptr;
		AuthoredTurretDropDef = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret_Nested")), RF_Transient);
		AuthoredTurretDropDef->Cost = 33.0f;
		AuthoredTurretDropDef->BuildingDefinition = TurretBuilding;
		Catalog.DebugForceUnresolvedNestedDefensiveTurretBuildingLoad(AuthoredTurretDropDef, TurretBuilding, true);
		Expect(Catalog.IsDropDefinitionPending(AuthoredTurretDropDef)
			&& Catalog.DebugGetCanonicalDefensiveTurretDrop() != AuthoredTurretDropDef,
			TEXT("U_TurretNestedPending"));

		if (IsValid(OwnerPS))
		{
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			GPBuildingDropAuthority::FPurchaseResult PendingTurret =
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, AuthoredTurretDropDef);
			Expect(!PendingTurret.bAccepted
				&& PendingTurret.RejectReason == EGP_BuildingDropRejectReason::DefinitionNotReady
				&& FMath::IsNearlyEqual(
					OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
					OrbitalBefore,
					0.05f),
				TEXT("U_TurretNestedDefinitionNotReadyNoSpend"));
		}

		Catalog.DebugCompletePendingNestedBuildingLoad();
		Expect(!Catalog.IsDropDefinitionPending(AuthoredTurretDropDef)
			&& Catalog.DebugGetCanonicalDefensiveTurretDrop() == AuthoredTurretDropDef
			&& FMath::IsNearlyEqual(Catalog.GetPurchaseCost(AuthoredTurretDropDef), 33.0f),
			TEXT("U_TurretNestedResolveReady"));

		if (IsValid(OwnerPS))
		{
			GPOrbitalBuildingDropDebug::GrantOrbital(OwnerPS, 50.0f);
			const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			GPBuildingDropAuthority::FPurchaseResult BuyTurret =
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, AuthoredTurretDropDef);
			Expect(BuyTurret.bAccepted
				&& FMath::IsNearlyEqual(BuyTurret.OrbitalCost, 33.0f, 0.05f)
				&& BuyTurret.ReadyAfter == 1
				&& FMath::IsNearlyEqual(
					OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
					OrbitalBefore - 33.0f,
					0.05f),
				TEXT("U_TurretAuthoredPurchaseReadyOnce"));
		}

		Catalog.DebugClearAuthoredBuildingDropOverrides();
		Catalog.DebugBeginContractIsolation();
		Expect(IsValid(Catalog.GetLegacyLogisticsHubDrop())
			&& Catalog.GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition() != nullptr
			&& !Catalog.IsDropDefinitionPending(Catalog.GetLegacyLogisticsHubDrop())
			&& FMath::IsNearlyEqual(Catalog.GetPurchaseCost(Catalog.GetLegacyLogisticsHubDrop()), 100.0f),
			TEXT("U_NativeHubBootstrapImmediatelyUsable"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12: // Catalog teardown: TryGetExisting never creates; BeginDestroy-style cleanup does not resurrect
	{
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == &Catalog
			&& IsValid(Catalog.GetLegacyLogisticsHubDrop()),
			TEXT("Life_GetBeforeShutdown"));

		RestoreSettings();
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == &Catalog
			&& !Catalog.IsContractIsolationActive(),
			TEXT("Life_NormalCleanupRestoresIsolation"));

		TSoftObjectPtr<UGP_OrbitalDropDefinition> SavedHubRef;
		TSoftObjectPtr<UGP_OrbitalDropDefinition> SavedTurretRef;
		TSoftObjectPtr<UGP_OrbitalDropDefinition> SavedWallRef;
		TSoftObjectPtr<UGP_OrbitalDropDefinition> SavedWallTurretRef;
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedHubRef = Settings->LogisticsHubDropDefinition;
			SavedTurretRef = Settings->DefensiveTurretDropDefinition;
			SavedWallRef = Settings->WallDropDefinition;
			SavedWallTurretRef = Settings->WallTurretDropDefinition;
		}

		UGP_OrbitalDropDefinition* NativeHub = Catalog.GetLegacyLogisticsHubDrop();
		UGP_BuildingDefinition* HubBuilding = NativeHub != nullptr
			? NativeHub->ResolveLoadedBuildingDefinition()
			: nullptr;
		UGP_OrbitalDropDefinition* PendingDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHUB_Teardown")), RF_Transient);
		PendingDrop->Cost = 23.0f;
		PendingDrop->BuildingDefinition = HubBuilding;
		Catalog.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(PendingDrop, HubBuilding, true);
		Expect(Catalog.IsDropDefinitionPending(PendingDrop) && Catalog.DebugIsCallbackSafe(),
			TEXT("Life_PendingBeforeShutdown"));

		TStrongObjectPtr<UGP_BuildingDropCatalog> Leftover;
		Leftover.Reset(&Catalog);
		UGP_BuildingDropCatalog::ShutdownCatalog();
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == nullptr, TEXT("Life_NullAfterShutdown"));
		Expect(Leftover.IsValid() && !Leftover->DebugIsCallbackSafe(), TEXT("Life_CallbackIgnoredAfterShutdown"));
		if (Leftover.IsValid())
		{
			Leftover->DebugCompletePendingNestedBuildingLoad();
		}
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == nullptr, TEXT("Life_CompletePendingNoResurrect"));

		RestoreSettings();
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == nullptr, TEXT("Life_DestroyCleanupNoResurrect"));
		Leftover.Reset();

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->LogisticsHubDropDefinition = SavedHubRef;
			Settings->DefensiveTurretDropDefinition = SavedTurretRef;
			Settings->WallDropDefinition = SavedWallRef;
			Settings->WallTurretDropDefinition = SavedWallTurretRef;
		}

		UGP_BuildingDropCatalog& Recreated = UGP_BuildingDropCatalog::Get();
		Recreated.DebugBeginContractIsolation();
		Expect(UGP_BuildingDropCatalog::TryGetExisting() == &Recreated
			&& IsValid(Recreated.GetLegacyLogisticsHubDrop()),
			TEXT("Life_RecreateAfterLifecycle"));

		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_OrbitalBuildingDropContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_OrbitalBuildingDropContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_OrbitalBuildingDropContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_OrbitalBuildingDropContractTestRunner::AdvanceStage() {}
bool UGP_OrbitalBuildingDropContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_OrbitalBuildingDropContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_OrbitalBuildingDropContractTestRunner::Finish() { bFinished = true; }
void UGP_OrbitalBuildingDropContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_OrbitalBuildingDropContractTestRunner::UnbindWorldCleanup() {}
void UGP_OrbitalBuildingDropContractTestRunner::CleanupActors() {}
void UGP_OrbitalBuildingDropContractTestRunner::RestoreSettings() {}
void UGP_OrbitalBuildingDropContractTestRunner::CleanupCatalogIfExists() {}

#endif
