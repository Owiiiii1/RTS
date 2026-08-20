// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPBuildingVitalsOwnershipContractTest.h"

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Units/GPUnitDefinition.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildingVitalsOwnershipContract, Log, All);

void AGP_BuildingVitalsOwnershipContractStub::ConfigureFallbacks(
	float MaxHealth,
	float Health,
	float Damage,
	float Armor,
	float Resistance,
	float Cooldown,
	float Range)
{
	DefaultMaxHealth = MaxHealth;
	DefaultHealth = Health;
	DefaultDamage = Damage;
	DefaultArmor = Armor;
	DefaultDamageResistance = Resistance;
	DefaultAttackCooldown = Cooldown;
	DefaultAttackRange = Range;
}

#if !UE_BUILD_SHIPPING

namespace GPBuildingVitalsOwnershipContractDebug
{
	static TWeakObjectPtr<UGP_BuildingVitalsOwnershipContractTestRunner> GActiveRunner;
	static const FVector Origin(-62000.0f, -18000.0f, 100.0f);

	static AGP_BuildingVitalsOwnershipContractStub* SpawnDeferred(UWorld* World, int32 Index)
	{
		return World->SpawnActorDeferred<AGP_BuildingVitalsOwnershipContractStub>(
			AGP_BuildingVitalsOwnershipContractStub::StaticClass(),
			FTransform(FRotator::ZeroRotator, Origin + FVector(Index * 500.0f, 0.0f, 0.0f)),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}

	static void FinishSpawn(AGP_BuildingVitalsOwnershipContractStub* Building, int32 Index)
	{
		if (Building != nullptr)
		{
			Building->FinishSpawning(
				FTransform(FRotator::ZeroRotator, Origin + FVector(Index * 500.0f, 0.0f, 0.0f)));
		}
	}

	static bool AttributesMatch(
		const AGP_BuildingVitalsOwnershipContractStub* Building,
		float MaxHealth,
		float Health,
		float Damage,
		float Armor,
		float Resistance,
		float Cooldown,
		float Range)
	{
		const UGP_UnitAttributeSet* Attrs = Building != nullptr ? Building->GetUnitAttributeSet() : nullptr;
		return Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), MaxHealth, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetHealth(), Health, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), Damage, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetArmor(), Armor, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamageResistance(), Resistance, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackCooldown(), Cooldown, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), Range, 0.01f);
	}

	static bool AttributesUninitialized(const AGP_BuildingVitalsOwnershipContractStub* Building)
	{
		return AttributesMatch(Building, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	static void RunBuildingVitalsOwnershipContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPBuildingVitalsOwnershipContract, Warning,
				TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPBuildingVitalsOwnershipContract, Warning,
				TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest: rejected - already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("BuildingVitalsOwnershipContract"), TEXT("BuildingVitalsOwnership"), Token))
		{
			return;
		}

		UGP_BuildingVitalsOwnershipContractTestRunner* Runner =
			NewObject<UGP_BuildingVitalsOwnershipContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GBuildingVitalsOwnershipContract(
		TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest"),
		TEXT("BuildingDefinition UnitDefinition precedence and GAS initialization contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunBuildingVitalsOwnershipContractTest));
}

void UGP_BuildingVitalsOwnershipContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_BuildingVitalsOwnershipContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_BuildingVitalsOwnershipContractTestRunner::OnWorldCleanup);

	CanonicalDefinition = NewObject<UGP_UnitDefinition>(this);
	CanonicalDefinition->MaxHealth = 333.0f;
	CanonicalDefinition->InitialHealth = 321.0f;
	CanonicalDefinition->Damage = 44.0f;
	CanonicalDefinition->Armor = 5.0f;
	CanonicalDefinition->DamageResistance = 0.25f;
	CanonicalDefinition->AttackCooldownSeconds = 2.0f;
	CanonicalDefinition->AttackRangeCm = 777.0f;
	CanonicalDefinition->SightRangeCm = 888.0f;

	ConflictingActorDefinition = NewObject<UGP_UnitDefinition>(this);
	ConflictingActorDefinition->MaxHealth = 888.0f;
	ConflictingActorDefinition->InitialHealth = 777.0f;
	ConflictingActorDefinition->Damage = 66.0f;
	ConflictingActorDefinition->Armor = 7.0f;
	ConflictingActorDefinition->DamageResistance = 0.5f;
	ConflictingActorDefinition->AttackCooldownSeconds = 3.0f;
	ConflictingActorDefinition->AttackRangeCm = 999.0f;

	CanonicalBuildingDefinition = NewObject<UGP_BuildingDefinition>(this);
	CanonicalBuildingDefinition->MaxHealth = 222.0f;
	CanonicalBuildingDefinition->UnitDefinition = CanonicalDefinition;

	FailingBuildingDefinition = NewObject<UGP_BuildingDefinition>(this);
	FailingBuildingDefinition->MaxHealth = 222.0f;
	FailingBuildingDefinition->UnitDefinition = TSoftObjectPtr<UGP_UnitDefinition>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Data/Units/DA_GP_Unit_MissingBuildingVitalsStub."
			"DA_GP_Unit_MissingBuildingVitalsStub")));

	EmptyBuildingDefinition = NewObject<UGP_BuildingDefinition>(this);
	EmptyBuildingDefinition->MaxHealth = 222.0f;
	EmptyBuildingDefinition->UnitDefinition.Reset();

	ScheduleNext();
}

void UGP_BuildingVitalsOwnershipContractTestRunner::ScheduleNext(float DelaySeconds)
{
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().SetTimer(
			StageTimerHandle,
			FTimerDelegate::CreateUObject(
				this, &UGP_BuildingVitalsOwnershipContractTestRunner::AdvanceStage),
			DelaySeconds,
			false);
	}
}

void UGP_BuildingVitalsOwnershipContractTestRunner::AdvanceStage()
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
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 0);
		if (!Expect(IsValid(Building), TEXT("A_SpawnCanonicalBuilding")))
		{
			Finish();
			return;
		}
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->UnitDefinitionAsset = ConflictingActorDefinition;
		Building->BuildingDefinitionAsset = CanonicalBuildingDefinition;
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 0);

		Expect(Building->ResolveLoadedBuildingDefinition() == CanonicalBuildingDefinition
			&& Building->ResolveLoadedUnitDefinition() == CanonicalDefinition
			&& Building->UnitDefinitionAsset.Get() == CanonicalDefinition,
			TEXT("A_BuildingDefinitionUnitDefinitionWinsActorDefinition"));
		Expect(GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
			Building, 333.0f, 321.0f, 44.0f, 5.0f, 0.25f, 2.0f, 777.0f),
			TEXT("A_CanonicalVitalsWinBuildingAndActorFallbacks"));
		Expect(Building->DebugGetCombatAttributesInitializationCount() == 1,
			TEXT("A_CanonicalGasInitializedExactlyOnce"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 1);
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->UnitDefinitionAsset = ConflictingActorDefinition;
		Building->DebugForceUnresolvedSoftBuildingDefinitionLoad(CanonicalBuildingDefinition, true);
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 1);

		Expect(Building->IsBuildingDefinitionLoadPending()
			&& !Building->IsBuildingDefinitionReady()
			&& !Building->IsUnitDefinitionReady()
			&& Building->DebugGetCombatAttributesInitializationCount() == 0
			&& GPBuildingVitalsOwnershipContractDebug::AttributesUninitialized(Building),
			TEXT("B_UnresolvedBuildingDefinitionDefersUnitGasInitialization"));
		Building->DebugCompletePendingBuildingDefinitionLoad();
		Expect(Building->ResolveLoadedUnitDefinition() == CanonicalDefinition
			&& GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
				Building, 333.0f, 321.0f, 44.0f, 5.0f, 0.25f, 2.0f, 777.0f)
			&& Building->DebugGetCombatAttributesInitializationCount() == 1,
			TEXT("B_BuildingDefinitionCompletionInitializesCanonicalGasOnce"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 2);
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->BuildingDefinitionAsset = CanonicalBuildingDefinition;
		Building->DebugForceUnresolvedSoftDefinitionLoad(CanonicalDefinition, true);
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 2);

		Expect(Building->IsBuildingDefinitionReady()
			&& Building->IsUnitDefinitionLoadPending()
			&& !Building->IsUnitDefinitionReady()
			&& Building->DebugGetCombatAttributesInitializationCount() == 0
			&& GPBuildingVitalsOwnershipContractDebug::AttributesUninitialized(Building),
			TEXT("C_UnresolvedNestedUnitDefinitionRemainsPendingWithoutDefaults"));
		Building->DebugCompletePendingUnitDefinitionLoad();
		Expect(Building->IsUnitDefinitionReady()
			&& GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
				Building, 333.0f, 321.0f, 44.0f, 5.0f, 0.25f, 2.0f, 777.0f)
			&& Building->DebugGetCombatAttributesInitializationCount() == 1,
			TEXT("C_NestedUnitDefinitionCompletionInitializesCanonicalGasOnce"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 3);
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->BuildingDefinitionAsset = FailingBuildingDefinition;
		Building->DebugForceUnresolvedSoftDefinitionLoad(nullptr, false);
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 3);
		FailureWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building = Buildings.IsValidIndex(3)
			? Buildings[3].Get()
			: nullptr;
		if (IsValid(Building) && !Building->IsUnitDefinitionReady() && FailureWaitTicks < 40)
		{
			++FailureWaitTicks;
			ScheduleNext();
			return;
		}
		Expect(IsValid(Building)
			&& Building->IsUnitDefinitionReady()
			&& !Building->IsUnitDefinitionLoadPending()
			&& GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
				Building, 111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f)
			&& Building->DebugGetCombatAttributesInitializationCount() == 1,
			TEXT("D_NestedUnitDefinitionLoadFailureUsesActorDefaultsWithoutHang"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 4);
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->UnitDefinitionAsset = ConflictingActorDefinition;
		Building->BuildingDefinitionAsset = EmptyBuildingDefinition;
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 4);
		Expect(Building->ResolveLoadedUnitDefinition() == ConflictingActorDefinition
			&& GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
				Building, 888.0f, 777.0f, 66.0f, 7.0f, 0.5f, 3.0f, 999.0f),
			TEXT("E_EmptyBuildingUnitDefinitionPreservesExplicitActorDefinition"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		AGP_BuildingVitalsOwnershipContractStub* Building =
			GPBuildingVitalsOwnershipContractDebug::SpawnDeferred(World, 5);
		Building->ConfigureFallbacks(111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f);
		Building->UnitDefinitionAsset.Reset();
		Building->BuildingDefinitionAsset = EmptyBuildingDefinition;
		Buildings.Add(Building);
		GPBuildingVitalsOwnershipContractDebug::FinishSpawn(Building, 5);
		Expect(Building->ResolveLoadedUnitDefinition() == nullptr
			&& GPBuildingVitalsOwnershipContractDebug::AttributesMatch(
				Building, 111.0f, 99.0f, 12.0f, 3.0f, 0.1f, 4.0f, 555.0f)
			&& Building->DebugGetCombatAttributesInitializationCount() == 1,
			TEXT("F_EmptyBuildingUnitDefinitionUsesActorDefaults"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

bool UGP_BuildingVitalsOwnershipContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPBuildingVitalsOwnershipContract, Error,
			TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPBuildingVitalsOwnershipContract, Log,
		TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest PASS: %s"), Label);
	return true;
}

void UGP_BuildingVitalsOwnershipContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	UE_LOG(LogGPBuildingVitalsOwnershipContract, Error,
		TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest ABORT: %s"), Reason);
	Finish();
}

void UGP_BuildingVitalsOwnershipContractTestRunner::Finish()
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
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPBuildingVitalsOwnershipContract, Log,
		TEXT("gp.Building.RunBuildingVitalsOwnershipContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPBuildingVitalsOwnershipContractDebug::GActiveRunner.Reset();
}

void UGP_BuildingVitalsOwnershipContractTestRunner::CleanupActors()
{
	for (const TWeakObjectPtr<AGP_BuildingVitalsOwnershipContractStub>& BuildingWeak : Buildings)
	{
		if (AGP_BuildingVitalsOwnershipContractStub* Building = BuildingWeak.Get())
		{
			Building->Destroy();
		}
	}
	Buildings.Reset();
}

void UGP_BuildingVitalsOwnershipContractTestRunner::OnWorldCleanup(
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

void UGP_BuildingVitalsOwnershipContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

#else

void UGP_BuildingVitalsOwnershipContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_BuildingVitalsOwnershipContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_BuildingVitalsOwnershipContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_BuildingVitalsOwnershipContractTestRunner::AdvanceStage() {}
bool UGP_BuildingVitalsOwnershipContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_BuildingVitalsOwnershipContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_BuildingVitalsOwnershipContractTestRunner::Finish() { bFinished = true; }
void UGP_BuildingVitalsOwnershipContractTestRunner::CleanupActors() {}
void UGP_BuildingVitalsOwnershipContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_BuildingVitalsOwnershipContractTestRunner::UnbindWorldCleanup() {}

#endif
