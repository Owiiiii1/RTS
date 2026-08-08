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
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingGroundPlacement.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"

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

void UGP_OrbitalBuildingDropContractTestRunner::RestoreSettings()
{
	if (!bSettingsMutated)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		Settings->BuildingDropDescentDurationSeconds = SavedBuildingDescent;
		Settings->BuildingDropCleanupDelaySeconds = SavedBuildingCleanup;
		Settings->BuildingDropSpawnAltitudeCm = SavedBuildingAltitude;
		Settings->BuildingDropPayloadDeployDelaySeconds = SavedBuildingDeployDelay;
		Settings->BuildingOrbitalPurchaseCost = SavedBuildingPurchaseCost;
		Settings->BuildingMaxDeployRadiusFromMainBaseCm = SavedBuildingMaxRadius;
		Settings->BuildingPayloadClass = SavedBuildingPayload;
	}
	bSettingsMutated = false;
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
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedBuildingDescent = Settings->BuildingDropDescentDurationSeconds;
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			SavedBuildingDeployDelay = Settings->BuildingDropPayloadDeployDelaySeconds;
			SavedBuildingPurchaseCost = Settings->BuildingOrbitalPurchaseCost;
			SavedBuildingMaxRadius = Settings->BuildingMaxDeployRadiusFromMainBaseCm;
			SavedBuildingPayload = Settings->BuildingPayloadClass;
			Settings->BuildingDropDescentDurationSeconds = 0.25f;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.0f;
			Settings->BuildingPayloadClass.Reset();
			bSettingsMutated = true;

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
	case 2: // D deploy without ready after consume setup + E radius + F overlap
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base), TEXT("D_ActorsAlive")))
		{
			Finish();
			return;
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
		Expect(Overlap.RejectReason == EGP_BuildingDropRejectReason::PlacementOverlap, TEXT("F_OverlapReason"));
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
		Expect(Deploy.SpawnedPod->GetPayloadKind() == EGP_DropPodPayloadKind::Building, TEXT("G_BuildingPayloadKind"));
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

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-48000.0f, 2000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		Base->SetTeamId(ContractTeam);
		ValidDeployLocation = Base->GetActorLocation() + FVector(900.0f, 0.0f, 0.0f);

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
		LastPodWeak = StubDeploy.SpawnedPod;

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

#endif
