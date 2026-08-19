// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPOrbitalDeliveryVisibilityContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPOrbitalDeliveryVisibility, Log, All);

namespace GPOrbitalDeliveryVisibilityDebug
{
	static TWeakObjectPtr<UGP_OrbitalDeliveryVisibilityContractTestRunner> GActiveRunner;

	static void RunOrbitalDeliveryVisibilityContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPOrbitalDeliveryVisibility, Warning,
				TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPOrbitalDeliveryVisibility, Warning,
				TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("OrbitalDeliveryVisibilityContract"), TEXT("SettingsVisibility"), Token))
		{
			return;
		}

		UGP_OrbitalDeliveryVisibilityContractTestRunner* Runner =
			NewObject<UGP_OrbitalDeliveryVisibilityContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GOrbitalDeliveryVisibilityContract(
		TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest"),
		TEXT("Slice A: GP Orbital Delivery Project Settings visibility/labeling contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunOrbitalDeliveryVisibilityContractTest));
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::OnWorldCleanup(
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

void UGP_OrbitalDeliveryVisibilityContractTestRunner::Finish()
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
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPOrbitalDeliveryVisibility, Log,
		TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPOrbitalDeliveryVisibilityDebug::GActiveRunner.Reset();
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPOrbitalDeliveryVisibility, Error,
		TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_OrbitalDeliveryVisibilityContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		UE_LOG(LogGPOrbitalDeliveryVisibility, Error,
			TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest FAIL: %s"), Label);
		++Failures;
	}
	return bOk;
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_OrbitalDeliveryVisibilityContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_OrbitalDeliveryVisibilityContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPOrbitalDeliveryVisibility, Log,
		TEXT("gp.Settings.RunOrbitalDeliveryVisibilityContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.05f);
}

void UGP_OrbitalDeliveryVisibilityContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	UClass* SettingsClass = UGP_OrbitalDeliverySettings::StaticClass();
	if (!Expect(SettingsClass != nullptr, TEXT("SettingsClass")))
	{
		Finish();
		return;
	}

	auto FindProp = [SettingsClass](const TCHAR* Name) -> FProperty*
	{
		return SettingsClass->FindPropertyByName(FName(Name));
	};

	auto ExpectCanonicalEdit = [this, &FindProp](const TCHAR* Name, const TCHAR* Label)
	{
		const FProperty* Prop = FindProp(Name);
		bool bOk = Prop != nullptr
			&& Prop->HasAnyPropertyFlags(CPF_Config)
			&& Prop->HasAnyPropertyFlags(CPF_Edit);
#if WITH_METADATA
		bOk = bOk && !Prop->HasMetaData(TEXT("DeprecatedProperty"));
#endif
		Expect(bOk, Label);
	};

	auto ExpectHiddenBridge = [this, &FindProp](const TCHAR* Name, const TCHAR* Label)
	{
		const FProperty* Prop = FindProp(Name);
		bool bOk = Prop != nullptr
			&& Prop->HasAnyPropertyFlags(CPF_Config)
			&& !Prop->HasAnyPropertyFlags(CPF_Edit);
#if WITH_METADATA
		bOk = bOk && Prop->HasMetaData(TEXT("DeprecatedProperty"));
#endif
		Expect(bOk, Label);
	};

	auto ExpectFallbackTiming = [this, &FindProp](
		const TCHAR* Name,
		const TCHAR* CategoryNeedle,
		const TCHAR* Label)
	{
		const FProperty* Prop = FindProp(Name);
		bool bOk = Prop != nullptr
			&& Prop->IsA<FFloatProperty>()
			&& Prop->HasAnyPropertyFlags(CPF_Config)
			&& Prop->HasAnyPropertyFlags(CPF_Edit);
#if WITH_METADATA
		const FString Category = Prop != nullptr ? Prop->GetMetaData(TEXT("Category")) : FString();
		const FString Display = Prop != nullptr ? Prop->GetMetaData(TEXT("DisplayName")) : FString();
		const FString Tip = Prop != nullptr ? Prop->GetMetaData(TEXT("ToolTip")) : FString();
		bOk = bOk
			&& Category.Contains(FString(CategoryNeedle))
			&& Category.Contains(TEXT("Fallback Defaults"))
			&& Display.Contains(TEXT("Fallback Seed"))
			&& (Tip.Contains(TEXT("Fallback seed")) || Tip.Contains(TEXT("normally overwrites")));
#else
		(void)CategoryNeedle;
#endif
		Expect(bOk, Label);
	};

	switch (StageIndex)
	{
	case 0:
	{
		ExpectCanonicalEdit(TEXT("WorkerDropDefinition"), TEXT("Canon_WorkerDropDefinition"));
		ExpectCanonicalEdit(TEXT("SalvageWalkerDropDefinition"), TEXT("Canon_SalvageWalkerDropDefinition"));
		ExpectCanonicalEdit(TEXT("LogisticsHubDropDefinition"), TEXT("Canon_LogisticsHubDropDefinition"));
		ExpectCanonicalEdit(TEXT("DefensiveTurretDropDefinition"), TEXT("Canon_DefensiveTurretDropDefinition"));
		ExpectCanonicalEdit(TEXT("WallDropDefinition"), TEXT("Canon_WallDropDefinition"));
		ExpectCanonicalEdit(TEXT("WallTurretDropDefinition"), TEXT("Canon_WallTurretDropDefinition"));
		ExpectCanonicalEdit(TEXT("WallPackageDefinition"), TEXT("Canon_WallPackageDefinition"));
		ExpectCanonicalEdit(TEXT("PodTransportSlotCapacity"), TEXT("Canon_PodTransportSlotCapacity"));
		ExpectCanonicalEdit(TEXT("UnitDropPodClass"), TEXT("Canon_UnitDropPodClass"));
		ExpectCanonicalEdit(TEXT("UnitDropSpawnAltitudeCm"), TEXT("Canon_UnitDropSpawnAltitudeCm"));
		ExpectCanonicalEdit(TEXT("UnitDropSpawnSpacingCm"), TEXT("Canon_UnitDropSpawnSpacingCm"));
		ExpectCanonicalEdit(TEXT("UnitDropCleanupDelaySeconds"), TEXT("Canon_UnitDropCleanupDelaySeconds"));
		ExpectCanonicalEdit(TEXT("BuildingDropSpawnAltitudeCm"), TEXT("Canon_BuildingDropSpawnAltitudeCm"));
		ExpectCanonicalEdit(TEXT("BuildingDropCleanupDelaySeconds"), TEXT("Canon_BuildingDropCleanupDelaySeconds"));
		ExpectCanonicalEdit(TEXT("BuildingMaxDeployRadiusFromMainBaseCm"), TEXT("Canon_BuildingMaxDeployRadius"));

		Expect(FindProp(TEXT("WorkerDropDefinition")) != nullptr
			&& FindProp(TEXT("WorkerDropDefinition"))->IsA<FSoftObjectProperty>(),
			TEXT("Type_WorkerDropDefinition"));
		Expect(FindProp(TEXT("PodTransportSlotCapacity")) != nullptr
			&& FindProp(TEXT("PodTransportSlotCapacity"))->IsA<FIntProperty>(),
			TEXT("Type_PodTransportSlotCapacity"));
		Expect(FindProp(TEXT("UnitDropSpawnAltitudeCm")) != nullptr
			&& FindProp(TEXT("UnitDropSpawnAltitudeCm"))->IsA<FFloatProperty>(),
			TEXT("Type_UnitDropSpawnAltitudeCm"));
		Expect(FindProp(TEXT("UnitDropPodClass")) != nullptr
			&& FindProp(TEXT("UnitDropPodClass"))->IsA<FSoftClassProperty>(),
			TEXT("Type_UnitDropPodClass"));

		Expect(FindProp(TEXT("WorkerTransportSlotCost")) == nullptr,
			TEXT("Absent_WorkerTransportSlotCost"));
		Expect(FindProp(TEXT("SalvageWalkerTransportSlotCost")) == nullptr,
			TEXT("Absent_SalvageWalkerTransportSlotCost"));
		Expect(FindProp(TEXT("WorkerOrbitalDropCost")) == nullptr,
			TEXT("Absent_WorkerOrbitalDropCost"));
		Expect(FindProp(TEXT("SalvageWalkerOrbitalDropCost")) == nullptr,
			TEXT("Absent_SalvageWalkerOrbitalDropCost"));
		ExpectHiddenBridge(TEXT("WorkerPayloadClass"), TEXT("Hidden_WorkerPayloadClass"));
		ExpectHiddenBridge(TEXT("SalvageWalkerPayloadClass"), TEXT("Hidden_SalvageWalkerPayloadClass"));
		ExpectHiddenBridge(TEXT("BuildingOrbitalPurchaseCost"), TEXT("Hidden_BuildingOrbitalPurchaseCost"));
		ExpectHiddenBridge(TEXT("BuildingPayloadClass"), TEXT("Hidden_BuildingPayloadClass"));
		Expect(FindProp(TEXT("BuildingPlacementOverlapMarginCm")) == nullptr,
			TEXT("Absent_BuildingPlacementOverlapMarginCm"));

		Expect(FindProp(TEXT("WorkerPayloadClass")) != nullptr
			&& FindProp(TEXT("WorkerPayloadClass"))->IsA<FSoftClassProperty>(),
			TEXT("Type_WorkerPayloadClass"));
		Expect(FindProp(TEXT("BuildingPayloadClass")) != nullptr
			&& FindProp(TEXT("BuildingPayloadClass"))->IsA<FSoftClassProperty>(),
			TEXT("Type_BuildingPayloadClass"));

		ExpectFallbackTiming(
			TEXT("UnitDropDescentDurationSeconds"),
			TEXT("Unit Product Timing"),
			TEXT("Fallback_UnitDropDescentDurationSeconds"));
		ExpectFallbackTiming(
			TEXT("UnitDropPayloadDeployDelaySeconds"),
			TEXT("Unit Product Timing"),
			TEXT("Fallback_UnitDropPayloadDeployDelaySeconds"));
		ExpectFallbackTiming(
			TEXT("BuildingDropDescentDurationSeconds"),
			TEXT("Building Product Timing"),
			TEXT("Fallback_BuildingDropDescentDurationSeconds"));
		ExpectFallbackTiming(
			TEXT("BuildingDropPayloadDeployDelaySeconds"),
			TEXT("Building Product Timing"),
			TEXT("Fallback_BuildingDropPayloadDeployDelaySeconds"));

		const FProperty* TurretPayload = FindProp(TEXT("DefensiveTurretPayloadClass"));
		bool bTurretOk = TurretPayload != nullptr
			&& TurretPayload->IsA<FSoftClassProperty>()
			&& TurretPayload->HasAnyPropertyFlags(CPF_Config)
			&& TurretPayload->HasAnyPropertyFlags(CPF_Edit);
#if WITH_METADATA
		const FString TurretCategory = TurretPayload != nullptr
			? TurretPayload->GetMetaData(TEXT("Category"))
			: FString();
		const FString TurretDisplay = TurretPayload != nullptr
			? TurretPayload->GetMetaData(TEXT("DisplayName"))
			: FString();
		bTurretOk = bTurretOk
			&& !TurretPayload->HasMetaData(TEXT("DeprecatedProperty"))
			&& TurretCategory.Contains(TEXT("LEGACY"))
			&& TurretDisplay.Contains(TEXT("LEGACY"));
#endif
		Expect(bTurretOk, TEXT("Legacy_DefensiveTurretPayloadClassEditable"));

		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		Expect(Settings != nullptr, TEXT("SettingsGet"));
		if (Settings != nullptr)
		{
			bool bUsedWorker = false;
			bool bUsedWalker = false;
			bool bUsedBuilding = false;
			bool bUsedTurret = false;
			Settings->ResolveWorkerPayloadClass(&bUsedWorker);
			Settings->ResolveSalvageWalkerPayloadClass(&bUsedWalker);
			Settings->ResolveBuildingPayloadClass(&bUsedBuilding);
			Settings->ResolveDefensiveTurretPayloadClass(&bUsedTurret);
			(void)bUsedWorker;
			(void)bUsedWalker;
			(void)bUsedBuilding;
			(void)bUsedTurret;
			Expect(true, TEXT("ReadersUnchangedSmoke"));
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

void UGP_OrbitalDeliveryVisibilityContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_OrbitalDeliveryVisibilityContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_OrbitalDeliveryVisibilityContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_OrbitalDeliveryVisibilityContractTestRunner::AdvanceStage() {}
bool UGP_OrbitalDeliveryVisibilityContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_OrbitalDeliveryVisibilityContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_OrbitalDeliveryVisibilityContractTestRunner::Finish() { bFinished = true; }
void UGP_OrbitalDeliveryVisibilityContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_OrbitalDeliveryVisibilityContractTestRunner::UnbindWorldCleanup() {}

#endif
