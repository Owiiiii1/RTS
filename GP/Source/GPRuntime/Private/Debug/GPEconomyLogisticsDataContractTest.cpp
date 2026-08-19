// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/GPEconomyLogisticsDataContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPEconomyLogistics, Log, All);

namespace GPEconomyLogisticsContractDebug
{
	static TWeakObjectPtr<UGP_EconomyLogisticsDataContractTestRunner> GActiveRunner;
	static const FVector Origin(-54000.0f, -24000.0f, 100.0f);
	constexpr int32 TeamId = 39;

	static AGP_PlayerState* SpawnTeamPlayerState(UWorld* World, AGameStateBase* GameState, int32 InTeamId)
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
		PS->SetTeamId(InTeamId);
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

	template <typename TActor>
	static TActor* SpawnDeferred(UWorld* World, const FVector& Loc)
	{
		return World->SpawnActorDeferred<TActor>(
			TActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, Loc),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}

	static void RunEconomyLogisticsDataContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPEconomyLogistics, Warning,
				TEXT("gp.Economy.RunEconomyLogisticsDataContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPEconomyLogistics, Warning,
				TEXT("gp.Economy.RunEconomyLogisticsDataContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("EconomyLogisticsDataContract"), TEXT("EconomyLogistics"), Token))
		{
			return;
		}

		UGP_EconomyLogisticsDataContractTestRunner* Runner =
			NewObject<UGP_EconomyLogisticsDataContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GEconomyLogisticsContract(
		TEXT("gp.Economy.RunEconomyLogisticsDataContractTest"),
		TEXT("GP-S39E economy / logistics data ownership contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunEconomyLogisticsDataContractTest));
}

void UGP_EconomyLogisticsDataContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_EconomyLogisticsDataContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_EconomyLogisticsDataContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_EconomyLogisticsDataContractTestRunner::CleanupActors()
{
	auto DestroyIf = [](AActor* Actor)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	};
	DestroyIf(WorkerDefWeak.Get());
	DestroyIf(WorkerFallbackWeak.Get());
	DestroyIf(MainBaseDefWeak.Get());
	DestroyIf(MainBaseFallbackWeak.Get());
	DestroyIf(MainBaseAsyncWeak.Get());
	DestroyIf(WorkerAsyncWeak.Get());
	DestroyIf(ResourceNodeWeak.Get());
	DestroyIf(ResourceNodeOverrideWeak.Get());
	DestroyIf(HubWeak.Get());
	DestroyIf(SpendPodWeak.Get());
	DestroyIf(SpendBaseWeak.Get());
	if (UWorld* World = WorldWeak.Get())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (AGP_PlayerState* PS = OwnerPSWeak.Get())
			{
				GS->RemovePlayerState(PS);
			}
		}
	}
	DestroyIf(OwnerPSWeak.Get());
	WorkerDefWeak.Reset();
	WorkerFallbackWeak.Reset();
	MainBaseDefWeak.Reset();
	MainBaseFallbackWeak.Reset();
	MainBaseAsyncWeak.Reset();
	WorkerAsyncWeak.Reset();
	ResourceNodeWeak.Reset();
	ResourceNodeOverrideWeak.Reset();
	HubWeak.Reset();
	SpendPodWeak.Reset();
	SpendBaseWeak.Reset();
	OwnerPSWeak.Reset();
	CargoOverrideDef = nullptr;
	StorageOverrideDef = nullptr;
	FerroniteDef = nullptr;
	AuthoredWorkerDropDef = nullptr;
	AuthoredHubDropDef = nullptr;
	AuthoredTurretDropDef = nullptr;
}

void UGP_EconomyLogisticsDataContractTestRunner::Finish()
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
	UGP_OrbitalUnitDropCatalog::Get().DebugClearAuthoredUnitDropOverrides();
	UGP_BuildingDropCatalog::Get().DebugClearAuthoredBuildingDropOverrides();
	UGP_OrbitalUnitDropCatalog::Get().OverrideDeliveryTiming(2.5f, 1.25f);
	UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(2.5f, 2.0f);
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPEconomyLogistics, Log,
		TEXT("gp.Economy.RunEconomyLogisticsDataContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPEconomyLogisticsContractDebug::GActiveRunner.Reset();
}

void UGP_EconomyLogisticsDataContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPEconomyLogistics, Error, TEXT("gp.Economy.RunEconomyLogisticsDataContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_EconomyLogisticsDataContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPEconomyLogistics, Error, TEXT("gp.Economy.RunEconomyLogisticsDataContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPEconomyLogistics, Log, TEXT("gp.Economy.RunEconomyLogisticsDataContractTest PASS: %s"), Label);
	return true;
}

void UGP_EconomyLogisticsDataContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_EconomyLogisticsDataContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_EconomyLogisticsDataContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_EconomyLogisticsDataContractTestRunner::OnWorldCleanup);
	ScheduleNext(0.05f);
}

void UGP_EconomyLogisticsDataContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		UGP_UnitDefinitionCatalog& Units = UGP_UnitDefinitionCatalog::Get();
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();

		const UGP_UnitDefinition* WorkerDef = Units.GetWorkerDefinition();
		const UGP_BuildingDefinition* MainBaseDef = Buildings.GetMainBaseBuilding();
		const UGP_BuildingDefinition* HubDef =
			IsValid(Buildings.GetLegacyLogisticsHubDrop())
				? Buildings.GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition()
				: nullptr;
		const UGP_OrbitalUnitDropDefinition* WorkerDrop = UnitDrops.GetWorkerDrop();
		const UGP_OrbitalUnitDropDefinition* WalkerDrop = UnitDrops.GetSalvageWalkerDrop();

		Expect(IsValid(WorkerDef) && FMath::IsNearlyEqual(WorkerDef->CargoCapacity, 50.0f), TEXT("Native_WorkerCargo50"));
		Expect(IsValid(Units.GetSalvageWalkerDefinition())
			&& FMath::IsNearlyEqual(Units.GetSalvageWalkerDefinition()->CargoCapacity, 0.0f),
			TEXT("Native_WalkerCargo0"));
		Expect(IsValid(MainBaseDef)
			&& FMath::IsNearlyEqual(MainBaseDef->ContainerCapacity, 100.0f)
			&& MainBaseDef->ContainerCount == 5, TEXT("Native_MainBaseStorage100x5"));
		Expect(IsValid(HubDef) && HubDef->UnitCapBonus == 5
			&& FMath::IsNearlyEqual(HubDef->ContainerCapacity, 0.0f)
			&& HubDef->ContainerCount == 0, TEXT("Native_HubBonus5NoStorage"));
		Expect(IsValid(WorkerDrop) && FMath::IsNearlyEqual(WorkerDrop->Cost, 25.0f)
			&& WorkerDrop->TransportSlotCost == 1
			&& FMath::IsNearlyEqual(WorkerDrop->DeliveryDescentSeconds, 2.5f)
			&& FMath::IsNearlyEqual(WorkerDrop->PayloadDeployDelaySeconds, 1.25f),
			TEXT("I_WorkerAcquisition25x1"));
		Expect(IsValid(WalkerDrop) && FMath::IsNearlyEqual(WalkerDrop->Cost, 50.0f)
			&& WalkerDrop->TransportSlotCost == 2, TEXT("J_WalkerAcquisition50x2"));

		FerroniteDef = NewObject<UGP_ResourceDefinition>(GetTransientPackage());
		Expect(IsValid(FerroniteDef)
			&& FMath::IsNearlyEqual(FerroniteDef->AmountPerMiningCycle, 10.0f)
			&& FMath::IsNearlyEqual(FerroniteDef->MiningCycleDurationSeconds, 1.0f)
			&& FMath::IsNearlyEqual(FerroniteDef->InteractionRangeCm, 200.0f)
			&& FMath::IsNearlyEqual(FerroniteDef->ScoreConversionRate, 1.0f)
			&& FMath::IsNearlyEqual(FerroniteDef->OrbitalConversionRate, 1.0f)
			&& FMath::IsNearlyEqual(FerroniteDef->ThreatPerStoredUnit, 0.5f)
			&& FerroniteDef->DepositMaxAmount == 5000
			&& FerroniteDef->MaxConcurrentMiners == 4, TEXT("G_FerroniteDefinitionBaseline"));

		const float ExpectedHubCost = 100.0f;
		Expect(FMath::IsNearlyEqual(Buildings.GetPurchaseCost(Buildings.GetLegacyLogisticsHubDrop()), ExpectedHubCost),
			TEXT("M_HubCost100"));
		TArray<UGP_OrbitalDropDefinition*> Drops;
		Buildings.GetOperatorVisibleDrops(Drops);
		bool bTurret150 = false;
		bool bWall25 = false;
		bool bWallTurret75 = false;
		for (UGP_OrbitalDropDefinition* Drop : Drops)
		{
			if (!IsValid(Drop))
			{
				continue;
			}
			const FName Name = Drop->GetPrimaryAssetId().PrimaryAssetName;
			if (Name == FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret")))
			{
				bTurret150 = FMath::IsNearlyEqual(Drop->Cost, 150.0f);
			}
			else if (Name == FName(TEXT("DA_GP_OrbitalDrop_Wall")))
			{
				bWall25 = FMath::IsNearlyEqual(Drop->Cost, 25.0f);
			}
			else if (Name == FName(TEXT("DA_GP_OrbitalDrop_WallTurret")))
			{
				bWallTurret75 = FMath::IsNearlyEqual(Drop->Cost, 75.0f);
			}
		}
		Expect(bTurret150 && bWall25 && bWallTurret75, TEXT("M_BuildingCostsUnchanged"));

		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		Expect(Settings != nullptr && Settings->PodTransportSlotCapacity == 4, TEXT("K_PodCapacity4Settings"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		UGP_UnitDefinition* WorkerDef = UGP_UnitDefinitionCatalog::Get().GetWorkerDefinition();
		AGP_Worker* Worker = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_Worker>(
			World, GPEconomyLogisticsContractDebug::Origin);
		if (Worker != nullptr)
		{
			Worker->UnitDefinitionAsset = WorkerDef;
			Worker->FinishSpawning(FTransform(
				FRotator::ZeroRotator, GPEconomyLogisticsContractDebug::Origin));
		}
		WorkerDefWeak = Worker;
		const UGP_CargoComponent* Cargo = IsValid(Worker) ? Worker->GetCargoComponent() : nullptr;
		Expect(IsValid(Worker) && Worker->IsUnitDefinitionReady()
			&& Cargo != nullptr && FMath::IsNearlyEqual(Cargo->GetCargoCapacity(), 50.0f),
			TEXT("A_WorkerDefinitionCargo50"));

		AGP_Worker* Fallback = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			GPEconomyLogisticsContractDebug::Origin + FVector(200.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator);
		WorkerFallbackWeak = Fallback;
		const UGP_CargoComponent* FallbackCargo = IsValid(Fallback) ? Fallback->GetCargoComponent() : nullptr;
		Expect(IsValid(Fallback) && Fallback->UnitDefinitionAsset.IsNull()
			&& FallbackCargo != nullptr && FMath::IsNearlyEqual(FallbackCargo->GetCargoCapacity(), 50.0f),
			TEXT("B_EmptyUnitDefinitionCargoFallback50"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		UGP_BuildingDefinition* MainBaseDef = UGP_BuildingDropCatalog::Get().GetMainBaseBuilding();
		AGP_MainBase* Base = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_MainBase>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(0.0f, 400.0f, 0.0f));
		if (Base != nullptr)
		{
			Base->BuildingDefinitionAsset = MainBaseDef;
			Base->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(0.0f, 400.0f, 0.0f)));
		}
		MainBaseDefWeak = Base;
		const UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
		Expect(IsValid(Base) && Base->IsBuildingDefinitionReady()
			&& Storage != nullptr && Storage->IsStorageConfigured()
			&& FMath::IsNearlyEqual(Storage->GetContainerCapacity(), 100.0f)
			&& Storage->GetContainerCount() == 5
			&& FMath::IsNearlyEqual(Storage->GetTotalCapacity(), 500.0f),
			TEXT("C_E_MainBaseDefinitionStorage100x5Total500"));

		AGP_MainBase* Fallback = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			GPEconomyLogisticsContractDebug::Origin + FVector(400.0f, 400.0f, 0.0f),
			FRotator::ZeroRotator);
		MainBaseFallbackWeak = Fallback;
		const UGP_StorageComponent* FallbackStorage = IsValid(Fallback) ? Fallback->GetStorageComponent() : nullptr;
		Expect(IsValid(Fallback) && Fallback->BuildingDefinitionAsset.IsNull()
			&& FallbackStorage != nullptr && FallbackStorage->IsStorageConfigured()
			&& FMath::IsNearlyEqual(FallbackStorage->GetContainerCapacity(), 100.0f)
			&& FallbackStorage->GetContainerCount() == 5, TEXT("D_EmptyBuildingDefinitionStorageFallback100x5"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		UGP_StorageComponent* Storage = IsValid(MainBaseDefWeak.Get())
			? MainBaseDefWeak->GetStorageComponent()
			: nullptr;
		if (!Expect(Storage != nullptr, TEXT("F_StoragePresent")))
		{
			Finish();
			return;
		}
		const FGP_StorageAddResult Partial = Storage->AddPlanetaryFerronite(99.0f);
		const int32 ReadyAfter99 = Storage->GetReadyCount();
		const FGP_StorageAddResult Rest = Storage->AddPlanetaryFerronite(1.0f);
		const int32 ReadyAfter100 = Storage->GetReadyCount();
		Expect(Partial.Accepted > 98.0f && ReadyAfter99 == 0, TEXT("F_99DoesNotBecomeReady"));
		Expect(Rest.Accepted > 0.0f && ReadyAfter100 == 1
			&& Storage->GetContainers().Num() > 0
			&& FMath::IsNearlyEqual(Storage->GetContainers()[0].CurrentAmount, 100.0f),
			TEXT("F_Full100BecomesReadyThresholdEqualsCapacity"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		AGP_ResourceNode* Node = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_ResourceNode>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(800.0f, 0.0f, 0.0f));
		if (Node != nullptr)
		{
			Node->SetResourceDefinitionSoft(FerroniteDef);
			Node->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(800.0f, 0.0f, 0.0f)));
		}
		ResourceNodeWeak = Node;
		Expect(IsValid(Node) && Node->GetMaxAmount() == 5000
			&& Node->GetCurrentAmount() == 5000
			&& Node->GetMaxConcurrentMiners() == 4, TEXT("H_ResourceNodeInitFromDefinition"));

		AGP_ResourceNode* OverrideNode = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_ResourceNode>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(1000.0f, 0.0f, 0.0f));
		if (OverrideNode != nullptr)
		{
			OverrideNode->SetResourceDefinitionSoft(FerroniteDef);
			OverrideNode->DebugAuthorDepositOverrideForTest(2000, 1500, 2);
			OverrideNode->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(1000.0f, 0.0f, 0.0f)));
		}
		ResourceNodeOverrideWeak = OverrideNode;
		Expect(IsValid(OverrideNode) && OverrideNode->GetMaxAmount() == 2000
			&& OverrideNode->GetCurrentAmount() == 1500
			&& OverrideNode->GetMaxConcurrentMiners() == 2,
			TEXT("H_AuthoredInstanceOverridePreserved"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 5:
	{
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		FGP_UnitDropManifest OneWalker;
		OneWalker.SalvageWalkerCount = 1;
		FGP_UnitDropManifest Overflow;
		Overflow.WorkerCount = 5;
		int32 Slots = 0;
		float Cost = 0.0f;
		int32 Units = 0;
		EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
		Expect(GPUnitDropAuthority::ComputeManifestCosts(OneWorker, Slots, Cost, Units, Reject)
			&& Slots == 1 && FMath::IsNearlyEqual(Cost, 25.0f), TEXT("I_ComputeWorker25x1"));
		Expect(GPUnitDropAuthority::ComputeManifestCosts(OneWalker, Slots, Cost, Units, Reject)
			&& Slots == 2 && FMath::IsNearlyEqual(Cost, 50.0f), TEXT("J_ComputeWalker50x2"));
		Expect(!GPUnitDropAuthority::ComputeManifestCosts(Overflow, Slots, Cost, Units, Reject)
			&& Reject == EGP_UnitDropRejectReason::SlotOverflow, TEXT("K_PodCapacity4RejectsOverflow"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 6:
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		float Descent = 0.0f;
		float Deploy = 0.0f;
		UnitDrops.ResolveManifestDeliveryTiming(OneWorker, Descent, Deploy);
		Expect(FMath::IsNearlyEqual(Descent, 2.5f) && FMath::IsNearlyEqual(Deploy, 1.25f),
			TEXT("N_UnitTimingFromAcquisitionDefinition"));

		UnitDrops.OverrideDeliveryTiming(0.4f, 0.1f);
		UnitDrops.ResolveManifestDeliveryTiming(OneWorker, Descent, Deploy);
		Expect(FMath::IsNearlyEqual(Descent, 0.4f) && FMath::IsNearlyEqual(Deploy, 0.1f),
			TEXT("N_UnitTimingDefinitionOverridesSettings"));
		UnitDrops.OverrideDeliveryTiming(2.5f, 1.25f);

		float BDescent = 0.0f;
		float BDeploy = 0.0f;
		Buildings.ResolveDeliveryTiming(Buildings.GetLegacyLogisticsHubDrop(), BDescent, BDeploy);
		Expect(FMath::IsNearlyEqual(BDescent, 2.5f) && FMath::IsNearlyEqual(BDeploy, 2.0f),
			TEXT("O_BuildingTimingFromDropDefinition"));
		Buildings.OverrideDeliveryTiming(0.3f, 0.05f);
		Buildings.ResolveDeliveryTiming(Buildings.GetLegacyLogisticsHubDrop(), BDescent, BDeploy);
		Expect(FMath::IsNearlyEqual(BDescent, 0.3f) && FMath::IsNearlyEqual(BDeploy, 0.05f),
			TEXT("O_BuildingTimingDefinitionOverridesSettings"));
		Buildings.OverrideDeliveryTiming(2.5f, 2.0f);
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7:
	{
		AGameStateBase* GS = World->GetGameState();
		AGP_PlayerState* PS = GPEconomyLogisticsContractDebug::SpawnTeamPlayerState(
			World, GS, GPEconomyLogisticsContractDebug::TeamId);
		OwnerPSWeak = PS;
		UGP_BuildingDefinition* HubDef =
			UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop() != nullptr
				? UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition()
				: nullptr;
		AGP_LogisticsHub* Hub = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_LogisticsHub>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(0.0f, 800.0f, 0.0f));
		if (Hub != nullptr)
		{
			Hub->BuildingDefinitionAsset = HubDef;
			Hub->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(0.0f, 800.0f, 0.0f)));
			if (IsValid(PS))
			{
				Hub->SetOwner(PS);
				Hub->SetTeamId(GPEconomyLogisticsContractDebug::TeamId);
			}
		}
		HubWeak = Hub;
		const UGP_PlayerAttributeSet* Attr = IsValid(PS) ? PS->GetPlayerAttributeSet() : nullptr;
		const float MaxUnits = Attr != nullptr ? Attr->GetMaxUnits() : -1.0f;
		Expect(IsValid(Hub) && IsValid(HubDef) && HubDef->UnitCapBonus == 5
			&& Hub->IsBuildingDefinitionReady()
			&& Attr != nullptr && FMath::IsNearlyEqual(MaxUnits, 10.0f),
			TEXT("P_HubUnitCapBonusFromBuildingDefinition"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		CargoOverrideDef = NewObject<UGP_UnitDefinition>(GetTransientPackage());
		CargoOverrideDef->DisplayName = FText::FromString(TEXT("CargoOverride"));
		CargoOverrideDef->CargoCapacity = 20.0f;
		CargoOverrideDef->MaxHealth = 100.0f;
		CargoOverrideDef->InitialHealth = 100.0f;

		AGP_Worker* AsyncWorker = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_Worker>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(-400.0f, 0.0f, 0.0f));
		if (AsyncWorker != nullptr)
		{
			AsyncWorker->DebugForceUnresolvedSoftDefinitionLoad(CargoOverrideDef, true);
			AsyncWorker->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(-400.0f, 0.0f, 0.0f)));
		}
		WorkerAsyncWeak = AsyncWorker;
		const UGP_CargoComponent* PendingCargo = IsValid(AsyncWorker) ? AsyncWorker->GetCargoComponent() : nullptr;
		Expect(IsValid(AsyncWorker) && AsyncWorker->IsUnitDefinitionLoadPending()
			&& !AsyncWorker->IsUnitDefinitionReady()
			&& PendingCargo != nullptr
			&& FMath::IsNearlyEqual(PendingCargo->GetCargoCapacity(), 50.0f),
			TEXT("Q_PendingUnitDefKeepsCargoFallbackUntilReady"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9:
	{
		AGP_Worker* AsyncWorker = WorkerAsyncWeak.Get();
		if (IsValid(AsyncWorker))
		{
			AsyncWorker->DebugCompletePendingUnitDefinitionLoad();
		}
		const UGP_CargoComponent* Cargo = IsValid(AsyncWorker) ? AsyncWorker->GetCargoComponent() : nullptr;
		Expect(IsValid(AsyncWorker) && AsyncWorker->IsUnitDefinitionReady()
			&& Cargo != nullptr && FMath::IsNearlyEqual(Cargo->GetCargoCapacity(), 20.0f),
			TEXT("Q_AsyncUnitDefAppliesCargo20NotSilent50"));

		StorageOverrideDef = NewObject<UGP_BuildingDefinition>(GetTransientPackage());
		StorageOverrideDef->DisplayName = FText::FromString(TEXT("StorageOverride"));
		StorageOverrideDef->ContainerCapacity = 40.0f;
		StorageOverrideDef->ContainerCount = 2;

		AGP_MainBase* AsyncBase = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_MainBase>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(-800.0f, 400.0f, 0.0f));
		if (AsyncBase != nullptr)
		{
			AsyncBase->DebugForceUnresolvedSoftBuildingDefinitionLoad(StorageOverrideDef, true);
			AsyncBase->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(-800.0f, 400.0f, 0.0f)));
		}
		MainBaseAsyncWeak = AsyncBase;
		const UGP_StorageComponent* PendingStorage = IsValid(AsyncBase) ? AsyncBase->GetStorageComponent() : nullptr;
		Expect(IsValid(AsyncBase) && AsyncBase->IsBuildingDefinitionLoadPending()
			&& !AsyncBase->IsBuildingDefinitionReady()
			&& PendingStorage != nullptr
			&& !PendingStorage->IsStorageConfigured(),
			TEXT("Q_PendingBuildingDefDoesNotLock100x5"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10:
	{
		AGP_MainBase* AsyncBase = MainBaseAsyncWeak.Get();
		if (IsValid(AsyncBase))
		{
			AsyncBase->DebugCompletePendingBuildingDefinitionLoad();
		}
		const UGP_StorageComponent* Storage = IsValid(AsyncBase) ? AsyncBase->GetStorageComponent() : nullptr;
		Expect(IsValid(AsyncBase) && AsyncBase->IsBuildingDefinitionReady()
			&& Storage != nullptr && Storage->IsStorageConfigured()
			&& FMath::IsNearlyEqual(Storage->GetContainerCapacity(), 40.0f)
			&& Storage->GetContainerCount() == 2
			&& FMath::IsNearlyEqual(Storage->GetTotalCapacity(), 80.0f),
			TEXT("Q_AsyncBuildingDefApplies40x2NotSilent100x5"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11:
	{
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		AGP_MainBase* Base = GPEconomyLogisticsContractDebug::SpawnDeferred<AGP_MainBase>(
			World, GPEconomyLogisticsContractDebug::Origin + FVector(1200.0f, 400.0f, 0.0f));
		if (Base != nullptr)
		{
			Base->FinishSpawning(FTransform(
				FRotator::ZeroRotator,
				GPEconomyLogisticsContractDebug::Origin + FVector(1200.0f, 400.0f, 0.0f)));
			if (IsValid(PS))
			{
				Base->SetOwner(PS);
				Base->SetTeamId(GPEconomyLogisticsContractDebug::TeamId);
			}
		}
		SpendBaseWeak = Base;
		if (IsValid(PS))
		{
			GPEconomyLogisticsContractDebug::GrantOrbital(PS, 100.0f);
			OrbitalBeforeSpend = PS->GetPlayerAttributeSet() != nullptr
				? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
				: 0.0f;
		}
		UGP_OrbitalUnitDropCatalog::Get().OverrideDeliveryTiming(0.05f, 0.0f);
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		const GPUnitDropAuthority::FEvalResult Result =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, PS, OneWorker);
		SpendPodWeak = Result.SpawnedPod.Get();
		const float OrbitalAfter = (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
			? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
			: -1.0f;
		Expect(Result.bAccepted && FMath::IsNearlyEqual(Result.OrbitalCost, 25.0f)
			&& FMath::IsNearlyEqual(OrbitalAfter, OrbitalBeforeSpend - 25.0f),
			TEXT("L_UnitDropExactSpend25Once"));
		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 12:
	{
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		const float OrbitalNow = (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
			? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
			: -1.0f;
		Expect(FMath::IsNearlyEqual(OrbitalNow, OrbitalBeforeSpend - 25.0f),
			TEXT("R_NoDuplicateSpend"));
		UGP_StorageComponent* Storage = IsValid(MainBaseDefWeak.Get())
			? MainBaseDefWeak->GetStorageComponent()
			: nullptr;
		Expect(Storage != nullptr && Storage->GetReadyCount() == 1, TEXT("R_NoDuplicateReady"));
		const UGP_PlayerAttributeSet* Attr = IsValid(PS) ? PS->GetPlayerAttributeSet() : nullptr;
		Expect(Attr != nullptr && FMath::IsNearlyEqual(Attr->GetMaxUnits(), 10.0f),
			TEXT("R_NoUnitCapRegression"));
		UGP_OrbitalUnitDropCatalog::Get().OverrideDeliveryTiming(2.5f, 1.25f);
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 13:
	{
		AuthoredWorkerDropDef = NewObject<UGP_OrbitalUnitDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalUnitDrop_Worker_Authored")), RF_Transient);
		AuthoredWorkerDropDef->Cost = 17.0f;
		AuthoredWorkerDropDef->TransportSlotCost = 3;
		AuthoredWorkerDropDef->DeliveryDescentSeconds = 4.25f;
		AuthoredWorkerDropDef->PayloadDeployDelaySeconds = 0.75f;
		AuthoredWorkerDropDef->UnitDefinition = UGP_UnitDefinitionCatalog::Get().GetWorkerDefinition();
		AuthoredWorkerDropDef->PayloadClass = AGP_Worker::StaticClass();

		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugClearAuthoredUnitDropOverrides();
		Expect(IsValid(UnitDrops.GetWorkerDrop())
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 25.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 1
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 50.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 2,
			TEXT("A_EmptyAuthoredRefUsesNativeBootstrap"));

		UnitDrops.DebugAssignLoadedAuthoredWorker(AuthoredWorkerDropDef);
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		float Descent = 0.0f;
		float Deploy = 0.0f;
		UnitDrops.ResolveManifestDeliveryTiming(OneWorker, Descent, Deploy);
		int32 Slots = 0;
		float Cost = 0.0f;
		int32 Units = 0;
		EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
		Expect(UnitDrops.GetWorkerDrop() == AuthoredWorkerDropDef
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 17.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 3
			&& FMath::IsNearlyEqual(Descent, 4.25f)
			&& FMath::IsNearlyEqual(Deploy, 0.75f)
			&& GPUnitDropAuthority::ComputeManifestCosts(OneWorker, Slots, Cost, Units, Reject)
			&& Slots == 3 && FMath::IsNearlyEqual(Cost, 17.0f)
			&& FMath::IsNearlyEqual(UnitDrops.GetSalvageWalkerOrbitalDropCost(), 50.0f)
			&& UnitDrops.GetSalvageWalkerTransportSlotCost() == 2,
			TEXT("B_LoadedAuthoredWorker17x3_4_25x0_75"));
		Expect(IsValid(UnitDrops.GetNativeWorkerDrop())
			&& FMath::IsNearlyEqual(UnitDrops.GetNativeWorkerDrop()->Cost, 25.0f)
			&& UnitDrops.GetNativeWorkerDrop()->TransportSlotCost == 1, TEXT("B_NativeBootstrapUnchanged25"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 14:
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugForceUnresolvedAuthoredWorkerLoad(AuthoredWorkerDropDef, true);
		Expect(UnitDrops.DebugDidRequestAsyncAuthoredWorkerLoad()
			&& UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == nullptr, TEXT("C_UnresolvedSoftRefRequestsAsyncLoad"));

		AGP_PlayerState* PS = OwnerPSWeak.Get();
		const float OrbitalBefore = (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
			? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
			: -1.0f;
		OrbitalBeforeSpend = OrbitalBefore;
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		int32 Slots = 0;
		float Cost = 0.0f;
		int32 Units = 0;
		EGP_UnitDropRejectReason Reject = EGP_UnitDropRejectReason::None;
		Expect(!GPUnitDropAuthority::ComputeManifestCosts(OneWorker, Slots, Cost, Units, Reject)
			&& Reject == EGP_UnitDropRejectReason::DefinitionNotReady, TEXT("D_PendingRejectsComputeNativeFallback"));
		const GPUnitDropAuthority::FEvalResult PendingResult =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, PS, OneWorker);
		const float OrbitalAfter = (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
			? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
			: -2.0f;
		Expect(!PendingResult.bAccepted
			&& PendingResult.RejectReason == EGP_UnitDropRejectReason::DefinitionNotReady
			&& !PendingResult.SpawnedPod.IsValid()
			&& FMath::IsNearlyEqual(OrbitalAfter, OrbitalBefore),
			TEXT("D_PendingDoesNotSpendReserveOrSpawn"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 15:
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugCompletePendingAuthoredWorkerLoad();
		FGP_UnitDropManifest OneWorker;
		OneWorker.WorkerCount = 1;
		float Descent = 0.0f;
		float Deploy = 0.0f;
		UnitDrops.ResolveManifestDeliveryTiming(OneWorker, Descent, Deploy);
		Expect(!UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == AuthoredWorkerDropDef
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 17.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 3
			&& FMath::IsNearlyEqual(Descent, 4.25f)
			&& FMath::IsNearlyEqual(Deploy, 0.75f),
			TEXT("E_CompletionSwitchesToAuthored17x3"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 16:
	{
		UGP_OrbitalUnitDropCatalog& UnitDrops = UGP_OrbitalUnitDropCatalog::Get();
		UnitDrops.DebugForceUnresolvedAuthoredWorkerLoad(AuthoredWorkerDropDef, true);
		UnitDrops.DebugForceAuthoredWorkerLoadFailure();
		Expect(UnitDrops.DebugConsumeWorkerLoadFailedLog()
			&& !UnitDrops.IsWorkerDropDefinitionPending()
			&& UnitDrops.GetWorkerDrop() == UnitDrops.GetNativeWorkerDrop()
			&& FMath::IsNearlyEqual(UnitDrops.GetWorkerOrbitalDropCost(), 25.0f)
			&& UnitDrops.GetWorkerTransportSlotCost() == 1, TEXT("F_FailedLoadLogsAndNativeFallback"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 17:
	{
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		AuthoredHubDropDef = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_Authored")), RF_Transient);
		AuthoredHubDropDef->Cost = 17.0f;
		AuthoredHubDropDef->DeliveryDescentSeconds = 4.25f;
		AuthoredHubDropDef->PayloadDeployDelaySeconds = 0.75f;
		AuthoredHubDropDef->BuildingDefinition = Buildings.GetLegacyLogisticsHubDrop() != nullptr
			? Buildings.GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition()
			: nullptr;
		Buildings.DebugAssignLoadedAuthoredLogisticsHub(AuthoredHubDropDef);
		float Descent = 0.0f;
		float Deploy = 0.0f;
		Buildings.ResolveDeliveryTiming(Buildings.GetLegacyLogisticsHubDrop(), Descent, Deploy);
		Expect(Buildings.GetLegacyLogisticsHubDrop() == AuthoredHubDropDef
			&& FMath::IsNearlyEqual(Buildings.GetPurchaseCost(AuthoredHubDropDef), 17.0f)
			&& FMath::IsNearlyEqual(Descent, 4.25f)
			&& FMath::IsNearlyEqual(Deploy, 0.75f),
			TEXT("BuildingAuthoredHubCost17NotNative100"));
		Buildings.DebugClearAuthoredBuildingDropOverrides();
		Expect(FMath::IsNearlyEqual(Buildings.GetPurchaseCost(Buildings.GetLegacyLogisticsHubDrop()), 100.0f),
			TEXT("BuildingEmptyRefRestoresNativeHub100"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 18:
	{
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		UGP_BuildingDefinition* HubBuilding = Buildings.GetLegacyLogisticsHubDrop() != nullptr
			? Buildings.GetLegacyLogisticsHubDrop()->ResolveLoadedBuildingDefinition()
			: nullptr;
		AuthoredHubDropDef = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_NestedPending")), RF_Transient);
		AuthoredHubDropDef->Cost = 17.0f;
		AuthoredHubDropDef->BuildingDefinition = HubBuilding;
		Buildings.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(AuthoredHubDropDef, HubBuilding, true);
		Expect(Buildings.DebugDidRequestAsyncNestedBuildingLoad()
			&& Buildings.IsDropDefinitionPending(AuthoredHubDropDef)
			&& Buildings.GetLegacyLogisticsHubDrop() != AuthoredHubDropDef,
			TEXT("NestedHub_PendingWhileBuildingUnloaded"));

		AGP_PlayerState* PS = OwnerPSWeak.Get();
		if (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
		{
			GPEconomyLogisticsContractDebug::GrantOrbital(PS, 50.0f);
			const float OrbitalBefore = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			const GPBuildingDropAuthority::FPurchaseResult Pending =
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, PS, AuthoredHubDropDef);
			const float OrbitalAfter = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			Expect(!Pending.bAccepted
				&& Pending.RejectReason == EGP_BuildingDropRejectReason::DefinitionNotReady
				&& FMath::IsNearlyEqual(OrbitalAfter, OrbitalBefore),
				TEXT("NestedHub_PurchaseDefinitionNotReadyNoSpend"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 19:
	{
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		Buildings.DebugCompletePendingNestedBuildingLoad();
		Expect(!Buildings.IsDropDefinitionPending(AuthoredHubDropDef)
			&& Buildings.GetLegacyLogisticsHubDrop() == AuthoredHubDropDef
			&& FMath::IsNearlyEqual(Buildings.GetPurchaseCost(AuthoredHubDropDef), 17.0f),
			TEXT("NestedHub_ReadyUsesAuthoredCost17"));

		AGP_PlayerState* PS = OwnerPSWeak.Get();
		if (IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
		{
			const float OrbitalBefore = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
			const GPBuildingDropAuthority::FPurchaseResult Buy =
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, PS, AuthoredHubDropDef);
			Expect(Buy.bAccepted
				&& FMath::IsNearlyEqual(Buy.OrbitalCost, 17.0f)
				&& Buy.ReadyAfter == 1
				&& FMath::IsNearlyEqual(PS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore - 17.0f),
				TEXT("NestedHub_PurchaseAcceptedReadyOnce"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 20:
	{
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		UGP_BuildingDefinition* HubBuilding = AuthoredHubDropDef != nullptr
			? AuthoredHubDropDef->ResolveLoadedBuildingDefinition()
			: nullptr;
		UGP_OrbitalDropDefinition* NullDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_NullBuilding")), RF_Transient);
		NullDrop->Cost = 21.0f;
		Buildings.DebugAssignLoadedAuthoredLogisticsHub(NullDrop);
		Expect(Buildings.DebugConsumeNullBuildingDefinitionLog()
			&& Buildings.GetLegacyLogisticsHubDrop() != NullDrop
			&& FMath::IsNearlyEqual(Buildings.GetPurchaseCost(Buildings.GetLegacyLogisticsHubDrop()), 100.0f),
			TEXT("NestedHub_NullBuildingNativeFallback"));

		UGP_OrbitalDropDefinition* FailedDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub_FailedNested")), RF_Transient);
		FailedDrop->Cost = 23.0f;
		FailedDrop->BuildingDefinition = HubBuilding;
		Buildings.DebugForceUnresolvedNestedLogisticsHubBuildingLoad(FailedDrop, HubBuilding, true);
		Buildings.DebugForceNestedBuildingLoadFailure();
		Expect(Buildings.DebugConsumeNestedBuildingLoadFailedLog()
			&& !Buildings.IsDropDefinitionPending(FailedDrop)
			&& Buildings.GetLegacyLogisticsHubDrop() != FailedDrop,
			TEXT("NestedHub_FailedNestedNativeFallback"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 21:
	{
		UGP_BuildingDropCatalog& Buildings = UGP_BuildingDropCatalog::Get();
		Buildings.DebugClearAuthoredBuildingDropOverrides();
		Buildings.DebugBeginContractIsolation();
		UGP_OrbitalDropDefinition* NativeTurret = Buildings.DebugGetCanonicalDefensiveTurretDrop();
		Expect(IsValid(NativeTurret)
			&& NativeTurret->ResolveLoadedBuildingDefinition() != nullptr
			&& !Buildings.IsDropDefinitionPending(NativeTurret),
			TEXT("NestedTurret_NativeImmediatelyUsable"));

		UGP_BuildingDefinition* TurretBuilding = NativeTurret != nullptr
			? NativeTurret->ResolveLoadedBuildingDefinition()
			: nullptr;
		AuthoredTurretDropDef = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret_Nested")), RF_Transient);
		AuthoredTurretDropDef->Cost = 33.0f;
		AuthoredTurretDropDef->BuildingDefinition = TurretBuilding;
		Buildings.DebugForceUnresolvedNestedDefensiveTurretBuildingLoad(AuthoredTurretDropDef, TurretBuilding, true);
		Expect(Buildings.IsDropDefinitionPending(AuthoredTurretDropDef), TEXT("NestedTurret_Pending"));
		Buildings.DebugCompletePendingNestedBuildingLoad();
		Expect(!Buildings.IsDropDefinitionPending(AuthoredTurretDropDef)
			&& Buildings.DebugGetCanonicalDefensiveTurretDrop() == AuthoredTurretDropDef
			&& FMath::IsNearlyEqual(Buildings.GetPurchaseCost(AuthoredTurretDropDef), 33.0f),
			TEXT("NestedTurret_ReadyAuthoredCost33"));
		Buildings.DebugEndContractIsolation();
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 22:
	{
		UGP_OrbitalUnitDropCatalog::Get().DebugClearAuthoredUnitDropOverrides();
		UGP_BuildingDropCatalog::Get().DebugClearAuthoredBuildingDropOverrides();
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_EconomyLogisticsDataContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_EconomyLogisticsDataContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_EconomyLogisticsDataContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_EconomyLogisticsDataContractTestRunner::AdvanceStage() {}
bool UGP_EconomyLogisticsDataContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_EconomyLogisticsDataContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_EconomyLogisticsDataContractTestRunner::Finish() { bFinished = true; }
void UGP_EconomyLogisticsDataContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_EconomyLogisticsDataContractTestRunner::UnbindWorldCleanup() {}
void UGP_EconomyLogisticsDataContractTestRunner::CleanupActors() {}

#endif
