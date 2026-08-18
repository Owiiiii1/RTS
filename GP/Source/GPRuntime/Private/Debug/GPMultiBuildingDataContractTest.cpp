// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPMultiBuildingDataContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameMode.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMultiBuildingData, Log, All);

namespace GPMultiBuildingDataDebug
{
	static TWeakObjectPtr<UGP_MultiBuildingDataContractTestRunner> GActiveRunner;
	constexpr int32 ContractTeam = 91;
	constexpr float CostA = 40.0f;
	constexpr float CostB = 70.0f;
	constexpr float CostC = 15.0f;

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

	static UGP_BuildingDefinition* MakeBuildingDef(
		UObject* Outer,
		FName Name,
		const FText& Display,
		TSubclassOf<AGP_BuildingBase> SpawnedClass)
	{
		UGP_BuildingDefinition* Def = NewObject<UGP_BuildingDefinition>(Outer, Name, RF_Transient);
		Def->DisplayName = Display;
		Def->MaxHealth = 250.0f;
		Def->FootprintCells = FIntPoint(2, 2);
		if (SpawnedClass)
		{
			Def->SpawnedClass = SpawnedClass;
		}
		UGP_BuildingDropCatalog::Get().RegisterBuildingDefinition(Def);
		return Def;
	}

	static UGP_OrbitalDropDefinition* MakeDropDef(
		UObject* Outer,
		FName Name,
		UGP_BuildingDefinition* Building,
		float Cost)
	{
		UGP_OrbitalDropDefinition* Drop = NewObject<UGP_OrbitalDropDefinition>(Outer, Name, RF_Transient);
		Drop->Cost = Cost;
		Drop->BuildingDefinition = Building;
		if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
		{
			Drop->DeliveryDescentSeconds = Settings->BuildingDropDescentDurationSeconds;
			Drop->PayloadDeployDelaySeconds = Settings->BuildingDropPayloadDeployDelaySeconds;
		}
		UGP_BuildingDropCatalog::Get().RegisterDropDefinition(Drop);
		return Drop;
	}

	static int32 CountBuckets(const UGP_OrbitalBuildingInventoryComponent* Inventory, const FPrimaryAssetId& Id)
	{
		int32 Count = 0;
		if (Inventory == nullptr)
		{
			return 0;
		}
		for (const FGP_ReadyBuildingEntry& Entry : Inventory->GetReadyEntries())
		{
			if (Entry.DropDefinitionId == Id)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountNativeCatalogDrops()
	{
		TArray<UGP_OrbitalDropDefinition*> Drops;
		UGP_BuildingDropCatalog::Get().GetOperatorVisibleDrops(Drops);
		int32 Valid = 0;
		for (UGP_OrbitalDropDefinition* Drop : Drops)
		{
			if (IsValid(Drop) && Drop->GetPrimaryAssetId().IsValid()
				&& IsValid(Drop->ResolveLoadedBuildingDefinition()))
			{
				++Valid;
			}
		}
		return Valid;
	}

	static bool NativeCatalogHasExpectedIdentities()
	{
		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		const FName ExpectedDrops[] = {
			FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub")),
			FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret")),
			FName(TEXT("DA_GP_OrbitalDrop_Wall")),
			FName(TEXT("DA_GP_OrbitalDrop_WallTurret"))
		};
		for (const FName& DropName : ExpectedDrops)
		{
			const FPrimaryAssetId Id(
				FPrimaryAssetType(UGP_OrbitalDropDefinition::PrimaryAssetTypeName()),
				DropName);
			UGP_OrbitalDropDefinition* Drop = Catalog.FindDropDefinition(Id);
			if (!IsValid(Drop) || Drop->GetPrimaryAssetId() != Id
				|| !IsValid(Drop->ResolveLoadedBuildingDefinition()))
			{
				return false;
			}
		}
		return true;
	}

	static int32 RoundMaxUnits(const AGP_PlayerState* PS)
	{
		if (PS == nullptr || PS->GetPlayerAttributeSet() == nullptr)
		{
			return 0;
		}
		return FMath::RoundToInt(PS->GetPlayerAttributeSet()->GetMaxUnits());
	}

	static void RunMultiBuildingDataContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMultiBuildingData, Warning, TEXT("gp.Building.RunMultiBuildingDataContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPMultiBuildingData, Warning, TEXT("gp.Building.RunMultiBuildingDataContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("MultiBuildingDataContract"), TEXT("MultiBuildingData"), Token))
		{
			return;
		}

		UGP_MultiBuildingDataContractTestRunner* Runner =
			NewObject<UGP_MultiBuildingDataContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMultiBuildingDataContract(
		TEXT("gp.Building.RunMultiBuildingDataContractTest"),
		TEXT("GP-S35B multi-building data architecture contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunMultiBuildingDataContractTest));
}

void UGP_MultiBuildingDataContractTestRunner::BeginDestroy()
{
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_MultiBuildingDataContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_MultiBuildingDataContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == WorldWeak.Get())
	{
		bCancelled = true;
		CancelReason = TEXT("WorldCleanup");
		Finish();
	}
}

void UGP_MultiBuildingDataContractTestRunner::RestoreSettings()
{
	if (!bSettingsMutated)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		Settings->BuildingDropPayloadDeployDelaySeconds = SavedBuildingDeployDelay;
		Settings->BuildingDropDescentDurationSeconds = SavedBuildingDescent;
		UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(2.5f, 2.0f);
		Settings->BuildingDropCleanupDelaySeconds = SavedBuildingCleanup;
		Settings->BuildingDropSpawnAltitudeCm = SavedBuildingAltitude;
	}
	bSettingsMutated = false;
}

void UGP_MultiBuildingDataContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_MultiBuildingDataContractStubA> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_MultiBuildingDataContractStubB> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			It->Destroy();
		}
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
		}
		if (AGP_PlayerState* PS = OwnerPSWeak.Get())
		{
			PS->Destroy();
		}
	}
	MainBaseWeak.Reset();
	OwnerPSWeak.Reset();
	LastPodWeak.Reset();
	LiveHubWeak.Reset();
}

void UGP_MultiBuildingDataContractTestRunner::Finish()
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
	UE_LOG(LogGPMultiBuildingData, Log,
		TEXT("gp.Building.RunMultiBuildingDataContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPMultiBuildingDataDebug::GActiveRunner.Reset();
}

void UGP_MultiBuildingDataContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPMultiBuildingData, Error,
		TEXT("gp.Building.RunMultiBuildingDataContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_MultiBuildingDataContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPMultiBuildingData, Error,
			TEXT("gp.Building.RunMultiBuildingDataContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPMultiBuildingData, Log,
		TEXT("gp.Building.RunMultiBuildingDataContractTest PASS: %s"), Label);
	return true;
}

void UGP_MultiBuildingDataContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_MultiBuildingDataContractTestRunner::AdvanceStage),
		FMath::Max(0.01f, DelaySeconds),
		false);
}

void UGP_MultiBuildingDataContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_MultiBuildingDataContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMultiBuildingData, Log, TEXT("gp.Building.RunMultiBuildingDataContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_MultiBuildingDataContractTestRunner::AdvanceStage()
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
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedBuildingDeployDelay = Settings->BuildingDropPayloadDeployDelaySeconds;
			SavedBuildingDescent = Settings->BuildingDropDescentDurationSeconds;
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.0f;
			Settings->BuildingDropDescentDurationSeconds = 0.2f;
			UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(
				Settings->BuildingDropDescentDurationSeconds,
				Settings->BuildingDropPayloadDeployDelaySeconds);
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			bSettingsMutated = true;
		}

		UGP_BuildingDropCatalog& Catalog = UGP_BuildingDropCatalog::Get();
		Expect(IsValid(&Catalog), TEXT("Catalog_Create"));
		const FPrimaryAssetId HubIdBefore = Catalog.GetLegacyLogisticsHubDropId();
		UGP_OrbitalDropDefinition* HubDropBefore = Catalog.GetLegacyLogisticsHubDrop();
		Expect(IsValid(HubDropBefore) && HubIdBefore.IsValid(), TEXT("Catalog_NativeAliveWhileOwned"));
		Expect(GPMultiBuildingDataDebug::CountNativeCatalogDrops() >= 4, TEXT("Catalog_FourNativeDrops"));
		Expect(GPMultiBuildingDataDebug::NativeCatalogHasExpectedIdentities(), TEXT("Catalog_ExpectedIdentities"));

		UGP_BuildingDropCatalog::ShutdownCatalog();
		UGP_BuildingDropCatalog::ShutdownCatalog();
		UGP_BuildingDropCatalog& Recreated = UGP_BuildingDropCatalog::Get();
		Expect(IsValid(&Recreated), TEXT("Catalog_RecreateAfterShutdown"));
		Expect(GPMultiBuildingDataDebug::NativeCatalogHasExpectedIdentities(), TEXT("Catalog_IdentitiesAfterRecreate"));
		Expect(Recreated.GetLegacyLogisticsHubDropId() == HubIdBefore, TEXT("Catalog_StableHubIdAfterRecreate"));

		UGP_BuildingDefinition* BuildingA = GPMultiBuildingDataDebug::MakeBuildingDef(
			this,
			FName(TEXT("DA_GP_Building_ContractA")),
			NSLOCTEXT("GPMultiBuilding", "A", "Contract A"),
			AGP_MultiBuildingDataContractStubA::StaticClass());
		UGP_BuildingDefinition* BuildingB = GPMultiBuildingDataDebug::MakeBuildingDef(
			this,
			FName(TEXT("DA_GP_Building_ContractB")),
			NSLOCTEXT("GPMultiBuilding", "B", "Contract B"),
			AGP_MultiBuildingDataContractStubB::StaticClass());
		UGP_BuildingDefinition* BuildingC = GPMultiBuildingDataDebug::MakeBuildingDef(
			this,
			FName(TEXT("DA_GP_Building_ContractC")),
			NSLOCTEXT("GPMultiBuilding", "C", "Contract C"),
			nullptr);

		UGP_OrbitalDropDefinition* DropA = GPMultiBuildingDataDebug::MakeDropDef(
			this, FName(TEXT("DA_GP_OrbitalDrop_ContractA")), BuildingA, GPMultiBuildingDataDebug::CostA);
		UGP_OrbitalDropDefinition* DropB = GPMultiBuildingDataDebug::MakeDropDef(
			this, FName(TEXT("DA_GP_OrbitalDrop_ContractB")), BuildingB, GPMultiBuildingDataDebug::CostB);
		UGP_OrbitalDropDefinition* DropC = GPMultiBuildingDataDebug::MakeDropDef(
			this, FName(TEXT("DA_GP_OrbitalDrop_ContractC")), BuildingC, GPMultiBuildingDataDebug::CostC);
		DropAWeak = DropA;
		DropBWeak = DropB;
		DropCWeak = DropC;

		Expect(DropA->IsA(UPrimaryDataAsset::StaticClass()) && DropA->GetPrimaryAssetId().IsValid(),
			TEXT("A_DropIsPrimaryDataAsset"));
		Expect(BuildingA->IsA(UPrimaryDataAsset::StaticClass()) && BuildingA->GetPrimaryAssetId().IsValid(),
			TEXT("A_BuildingIsPrimaryDataAsset"));
		Expect(DropA->GetPrimaryAssetId() != DropB->GetPrimaryAssetId(), TEXT("D_DistinctDropIdentities"));
		Expect(BuildingA->GetPrimaryAssetId() != BuildingB->GetPrimaryAssetId(), TEXT("D_DistinctBuildingIdentities"));
		Expect(DropA->ResolveLoadedBuildingDefinition() == BuildingA, TEXT("B_SoftBuildingIdentity"));
		Expect(!FMath::IsNearlyEqual(DropA->Cost, DropB->Cost), TEXT("D_DifferentCosts"));
		Expect(BuildingA->ResolveLoadedSpawnedClass() == AGP_MultiBuildingDataContractStubA::StaticClass(),
			TEXT("D_PayloadClassA"));
		Expect(BuildingB->ResolveLoadedSpawnedClass() == AGP_MultiBuildingDataContractStubB::StaticClass(),
			TEXT("D_PayloadClassB"));
		Expect(FMath::IsNearlyEqual(GPBuildingDropAuthority::GetPurchaseCost(DropA), DropA->Cost),
			TEXT("C_CostFromDropDef"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-50000.0f, 4000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(GPMultiBuildingDataDebug::ContractTeam);
		ValidDeployLocation = Base->GetActorLocation() + FVector(900.0f, 0.0f, 0.0f);

		AGP_PlayerState* OwnerPS = GPMultiBuildingDataDebug::SpawnTeamPlayerState(
			World, GS, GPMultiBuildingDataDebug::ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS) && OwnerPS->GetOrbitalBuildingInventoryComponent() != nullptr, TEXT("SpawnOwnerPS")))
		{
			Finish();
			return;
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		Expect(Inventory->AuthorityAddReady(DropA, 2), TEXT("E_AddA"));
		Expect(Inventory->AuthorityAddReady(DropB, 3), TEXT("E_AddB"));
		Expect(Inventory->AuthorityAddReady(DropC, 1), TEXT("E_AddC"));
		Expect(Inventory->GetReadyCount(DropA) == 2, TEXT("E_CountA"));
		Expect(Inventory->GetReadyCount(DropB) == 3, TEXT("E_CountB"));
		Expect(Inventory->GetReadyCount(DropC) == 1, TEXT("E_CountC"));
		Expect(Inventory->AuthorityAddReady(DropA, 1), TEXT("E_AddAAgain"));
		Expect(Inventory->GetReadyCount(DropA) == 3 && Inventory->GetReadyCount(DropB) == 3, TEXT("E_IndependentAdd"));
		Expect(GPMultiBuildingDataDebug::CountBuckets(Inventory, DropA->GetPrimaryAssetId()) == 1,
			TEXT("E_NoDuplicateBucket"));
		Expect(Inventory->AuthorityTryConsumeReady(DropB, 1), TEXT("E_ConsumeB"));
		Expect(Inventory->GetReadyCount(DropB) == 2 && Inventory->GetReadyCount(DropA) == 3, TEXT("E_IndependentConsume"));
		Expect(!Inventory->AuthorityTryConsumeReady(DropC, 4), TEXT("E_NoNegative"));
		Expect(Inventory->GetReadyCount(DropC) == 1, TEXT("E_CUnchangedOnFailedConsume"));
		Inventory->AuthorityTryConsumeReady(DropA, 3);
		Inventory->AuthorityTryConsumeReady(DropB, 2);
		Inventory->AuthorityTryConsumeReady(DropC, 1);
		Expect(Inventory->GetReadyCount(DropA) == 0 && Inventory->GetReadyEntries().Num() == 0, TEXT("E_Cleared"));

		Expect(Inventory->GetReadyCount(EGP_OrbitalBuildingType::None) == 0, TEXT("M_EnumNoneNotCanonical"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* DropA = DropAWeak.Get();
		UGP_OrbitalDropDefinition* DropB = DropBWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(DropA) && IsValid(DropB), TEXT("F_SetupAlive")))
		{
			Finish();
			return;
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		GPBuildingDropAuthority::FPurchaseResult Poor =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, DropA);
		Expect(!Poor.bAccepted, TEXT("F_InsufficientReject"));
		Expect(Poor.RejectReason == EGP_BuildingDropRejectReason::InsufficientOrbital, TEXT("F_InsufficientReason"));
		Expect(Inventory->GetReadyCount(DropA) == 0, TEXT("F_NoReadyOnInsufficient"));

		GPMultiBuildingDataDebug::GrantOrbital(OwnerPS, 500.0f);
		OrbitalBeforePurchaseA = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();

		GPBuildingDropAuthority::FPurchaseResult BuyA =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, DropA);
		Expect(BuyA.bAccepted, TEXT("F_PurchaseA"));
		Expect(Inventory->GetReadyCount(DropA) == 1 && Inventory->GetReadyCount(DropB) == 0, TEXT("F_ReadyAOnly"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBeforePurchaseA - GPMultiBuildingDataDebug::CostA,
			0.05f), TEXT("F_SpentCostA"));

		const float AfterA = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		GPBuildingDropAuthority::FPurchaseResult BuyB =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, DropB);
		Expect(BuyB.bAccepted, TEXT("F_PurchaseB"));
		Expect(Inventory->GetReadyCount(DropA) == 1 && Inventory->GetReadyCount(DropB) == 1, TEXT("F_ReadyAB"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			AfterA - GPMultiBuildingDataDebug::CostB,
			0.05f), TEXT("F_SpentCostB"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* DropA = DropAWeak.Get();
		UGP_OrbitalDropDefinition* DropB = DropBWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(DropA) && IsValid(DropB), TEXT("G_SetupAlive")))
		{
			Finish();
			return;
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		OrbitalBeforeDeployA = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const FTransform DeployA(FRotator::ZeroRotator, ValidDeployLocation);
		GPBuildingDropAuthority::FDeployResult ResultA =
			GPBuildingDropAuthority::AuthorityDeployBuilding(World, OwnerPS, DropA, DeployA);
		Expect(ResultA.bAccepted, TEXT("G_DeployA"));
		Expect(ResultA.PayloadClass == AGP_MultiBuildingDataContractStubA::StaticClass(), TEXT("G_PayloadFromBuildingDef"));
		Expect(Inventory->GetReadyCount(DropA) == 0 && Inventory->GetReadyCount(DropB) == 1, TEXT("G_ConsumeANotB"));
		Expect(FMath::IsNearlyEqual(
			OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBeforeDeployA,
			0.05f), TEXT("G_NoSecondSpend"));
		LastPodWeak = ResultA.SpawnedPod;

		GPBuildingDropAuthority::FDeployResult DupA =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				DropA,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation + FVector(0.0f, 450.0f, 0.0f)));
		Expect(!DupA.bAccepted, TEXT("H_DuplicateReject"));
		Expect(DupA.RejectReason == EGP_BuildingDropRejectReason::NoReadyInventory, TEXT("H_DuplicateReason"));
		Expect(Inventory->GetReadyCount(DropB) == 1, TEXT("H_BUnchanged"));

		++StageIndex;
		ScheduleNext(0.25f);
		break;
	}
	case 3:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* DropC = DropCWeak.Get();
		if (!Expect(IsValid(OwnerPS) && IsValid(DropC), TEXT("I_SetupAlive")))
		{
			Finish();
			return;
		}

		int32 StubACount = 0;
		int32 StubATeam = 0;
		for (TActorIterator<AGP_MultiBuildingDataContractStubA> It(World); It; ++It)
		{
			++StubACount;
			StubATeam = It->GetTeamId();
		}
		Expect(StubACount == 1, TEXT("G_PayloadSpawnedOnce"));
		Expect(StubATeam == GPMultiBuildingDataDebug::ContractTeam, TEXT("G_TeamPreserved"));

		UGP_OrbitalDropDefinition* MissingBuildingDrop = GPMultiBuildingDataDebug::MakeDropDef(
			this, FName(TEXT("DA_GP_OrbitalDrop_ContractMissingBuilding")), nullptr, 20.0f);
		GPBuildingDropAuthority::FPurchaseResult MissingBuilding =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, MissingBuildingDrop);
		Expect(!MissingBuilding.bAccepted, TEXT("I_MissingBuildingReject"));
		Expect(MissingBuilding.RejectReason == EGP_BuildingDropRejectReason::MissingBuildingDefinition,
			TEXT("I_MissingBuildingReason"));

		GPBuildingDropAuthority::FPurchaseResult NullDrop =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(
				World, OwnerPS, static_cast<UGP_OrbitalDropDefinition*>(nullptr));
		Expect(!NullDrop.bAccepted && NullDrop.RejectReason == EGP_BuildingDropRejectReason::InvalidDefinition,
			TEXT("I_InvalidDefinition"));

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		GPBuildingDropAuthority::FPurchaseResult BuyC =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, DropC);
		Expect(BuyC.bAccepted, TEXT("J_PurchaseNoClass"));
		GPBuildingDropAuthority::FDeployResult DeployC =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				DropC,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation + FVector(0.0f, 800.0f, 0.0f)));
		Expect(!DeployC.bAccepted, TEXT("J_MissingClassReject"));
		Expect(DeployC.RejectReason == EGP_BuildingDropRejectReason::MissingSpawnedClass, TEXT("J_MissingClassReason"));
		Expect(Inventory->GetReadyCount(DropC) == 1, TEXT("J_ReadyPreserved"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("K_OwnerAlive")))
		{
			Finish();
			return;
		}

		MaxUnitsBeforeHub = GPMultiBuildingDataDebug::RoundMaxUnits(OwnerPS);
		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		Expect(IsValid(HubDrop), TEXT("K_HubDropPresent"));
		Expect(HubDrop->GetPrimaryAssetId().IsValid(), TEXT("M_HubUsesPrimaryAssetId"));

		GPBuildingDropAuthority::FPurchaseResult HubBuy =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(
				World, OwnerPS, EGP_OrbitalBuildingType::LogisticsHub);
		Expect(HubBuy.bAccepted, TEXT("K_HubPurchase"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(HubDrop) == 1, TEXT("K_HubReadyByDef"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub) == 1,
			TEXT("K_HubReadyByEnumGlue"));

		const FVector HubLoc = ValidDeployLocation + FVector(0.0f, 1200.0f, 0.0f);
		GPBuildingDropAuthority::FDeployResult HubDeploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(FRotator::ZeroRotator, HubLoc));
		Expect(HubDeploy.bAccepted, TEXT("K_HubDeploy"));
		LastPodWeak = HubDeploy.SpawnedPod;

		++StageIndex;
		ScheduleNext(0.3f);
		break;
	}
	case 5:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("K_OwnerAliveAfterHub")))
		{
			Finish();
			return;
		}

		AGP_LogisticsHub* Hub = nullptr;
		int32 HubCount = 0;
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPMultiBuildingDataDebug::ContractTeam)
			{
				++HubCount;
				Hub = *It;
			}
		}
		Expect(HubCount == 1 && IsValid(Hub), TEXT("K_LiveHub"));
		LiveHubWeak = Hub;
		Expect(GPMultiBuildingDataDebug::RoundMaxUnits(OwnerPS) == MaxUnitsBeforeHub + 5, TEXT("K_Plus5Granted"));

		if (IsValid(Hub))
		{
			Hub->Destroy();
		}
		Expect(GPMultiBuildingDataDebug::RoundMaxUnits(OwnerPS) == MaxUnitsBeforeHub, TEXT("K_Plus5Removed"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* DropB = DropBWeak.Get();
		AGP_GameMode* GM = World->GetAuthGameMode<AGP_GameMode>();
		if (!Expect(IsValid(OwnerPS) && IsValid(DropB) && IsValid(GM), TEXT("L_SetupAlive")))
		{
			Finish();
			return;
		}

		GM->FinishMatch(GPMultiBuildingDataDebug::ContractTeam, FGPGameplayTags::Get().Match_WinReason_DeliveryQuota);
		Expect(GS->IsMatchFinished(), TEXT("L_MatchFinished"));

		const int32 ReadyB = OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(DropB);
		GPBuildingDropAuthority::FPurchaseResult FinishedBuy =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, DropB);
		Expect(!FinishedBuy.bAccepted, TEXT("L_PurchaseFinishedReject"));
		Expect(FinishedBuy.RejectReason == EGP_BuildingDropRejectReason::MatchFinished, TEXT("L_PurchaseFinishedReason"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(DropB) == ReadyB, TEXT("L_ReadyUnchanged"));

		GPBuildingDropAuthority::FDeployResult FinishedDeploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				DropB,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation + FVector(0.0f, 1600.0f, 0.0f)));
		Expect(!FinishedDeploy.bAccepted, TEXT("L_DeployFinishedReject"));
		Expect(FinishedDeploy.RejectReason == EGP_BuildingDropRejectReason::MatchFinished, TEXT("L_DeployFinishedReason"));

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_MultiBuildingDataContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_MultiBuildingDataContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_MultiBuildingDataContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_MultiBuildingDataContractTestRunner::AdvanceStage() {}
bool UGP_MultiBuildingDataContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_MultiBuildingDataContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_MultiBuildingDataContractTestRunner::Finish() { bFinished = true; }
void UGP_MultiBuildingDataContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_MultiBuildingDataContractTestRunner::UnbindWorldCleanup() {}
void UGP_MultiBuildingDataContractTestRunner::CleanupActors() {}
void UGP_MultiBuildingDataContractTestRunner::RestoreSettings() {}

#endif
