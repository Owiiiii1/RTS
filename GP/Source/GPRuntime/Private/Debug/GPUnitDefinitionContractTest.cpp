// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitDefinitionContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "TimerManager.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitDefinitionContract, Log, All);

namespace GPUnitDefinitionContractDebug
{
	static TWeakObjectPtr<UGP_UnitDefinitionContractTestRunner> GActiveRunner;
	static const FVector Origin(-54000.0f, -16000.0f, 100.0f);

	template <typename TActor>
	static TActor* SpawnDeferredBase(UWorld* World, const FVector& Loc)
	{
		return World->SpawnActorDeferred<TActor>(
			TActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, Loc),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}

	template <typename TActor>
	static TActor* SpawnWithDefinition(UWorld* World, const FVector& Loc, UGP_UnitDefinition* Def)
	{
		TActor* Actor = SpawnDeferredBase<TActor>(World, Loc);
		if (Actor != nullptr)
		{
			Actor->UnitDefinitionAsset = Def;
			Actor->FinishSpawning(FTransform(FRotator::ZeroRotator, Loc));
		}
		return Actor;
	}

	template <typename TActor>
	static TActor* SpawnWithForcedAsync(
		UWorld* World,
		const FVector& Loc,
		UGP_UnitDefinition* InjectedDef,
		bool bHoldCompletion)
	{
		TActor* Actor = SpawnDeferredBase<TActor>(World, Loc);
		if (Actor != nullptr)
		{
			Actor->DebugForceUnresolvedSoftDefinitionLoad(InjectedDef, bHoldCompletion);
			Actor->FinishSpawning(FTransform(FRotator::ZeroRotator, Loc));
		}
		return Actor;
	}

	static void RunUnitDefinitionContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPUnitDefinitionContract, Warning,
				TEXT("gp.Units.RunUnitDefinitionContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPUnitDefinitionContract, Warning,
				TEXT("gp.Units.RunUnitDefinitionContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("UnitDefinitionContract"), TEXT("UnitDefinition"), Token))
		{
			return;
		}

		UGP_UnitDefinitionContractTestRunner* Runner =
			NewObject<UGP_UnitDefinitionContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GUnitDefinitionContract(
		TEXT("gp.Units.RunUnitDefinitionContractTest"),
		TEXT("GP-S38D UnitDefinition PrimaryAsset + initialization contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunUnitDefinitionContractTest));
}

void UGP_UnitDefinitionContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_UnitDefinitionContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_UnitDefinitionContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_UnitDefinitionContractTestRunner::CleanupActors()
{
	if (AGP_SalvageWalker* SW = WalkerWeak.Get())
	{
		SW->Destroy();
	}
	if (AGP_SalvageWalker* SW = FallbackWalkerWeak.Get())
	{
		SW->Destroy();
	}
	if (AGP_SalvageWalker* SW = OverrideWalkerWeak.Get())
	{
		SW->Destroy();
	}
	if (AGP_DefensiveTurret* Turret = TurretWeak.Get())
	{
		Turret->Destroy();
	}
	if (AGP_Worker* Worker = WorkerWeak.Get())
	{
		Worker->Destroy();
	}
	if (AGP_SalvageWalker* SW = AsyncWalkerWeak.Get())
	{
		SW->Destroy();
	}
	if (AGP_SalvageWalker* SW = FailWalkerWeak.Get())
	{
		SW->Destroy();
	}
	WalkerWeak.Reset();
	FallbackWalkerWeak.Reset();
	OverrideWalkerWeak.Reset();
	TurretWeak.Reset();
	WorkerWeak.Reset();
	AsyncWalkerWeak.Reset();
	FailWalkerWeak.Reset();
	OverrideDef = nullptr;
	AsyncDef = nullptr;
}

void UGP_UnitDefinitionContractTestRunner::Finish()
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
	UE_LOG(LogGPUnitDefinitionContract, Log,
		TEXT("gp.Units.RunUnitDefinitionContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPUnitDefinitionContractDebug::GActiveRunner.Reset();
}

void UGP_UnitDefinitionContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPUnitDefinitionContract, Error, TEXT("gp.Units.RunUnitDefinitionContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_UnitDefinitionContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPUnitDefinitionContract, Error, TEXT("gp.Units.RunUnitDefinitionContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPUnitDefinitionContract, Log, TEXT("gp.Units.RunUnitDefinitionContractTest PASS: %s"), Label);
	return true;
}

void UGP_UnitDefinitionContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_UnitDefinitionContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_UnitDefinitionContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_UnitDefinitionContractTestRunner::OnWorldCleanup);
	ScheduleNext(0.05f);
}

void UGP_UnitDefinitionContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	UGP_UnitDefinitionCatalog& Catalog = UGP_UnitDefinitionCatalog::Get();
	UGP_UnitDefinition* WalkerDef = Catalog.GetSalvageWalkerDefinition();
	UGP_UnitDefinition* TurretDef = Catalog.GetDefensiveTurretDefinition();
	UGP_UnitDefinition* WorkerDef = Catalog.GetWorkerDefinition();

	switch (StageIndex)
	{
	case 0:
	{
		Expect(IsValid(WalkerDef) && WalkerDef->GetPrimaryAssetId().IsValid()
			&& WalkerDef->GetPrimaryAssetId().PrimaryAssetType
				== FPrimaryAssetType(UGP_UnitDefinition::PrimaryAssetTypeName())
			&& WalkerDef->GetPrimaryAssetId().PrimaryAssetName == FName(TEXT("DA_GP_Unit_SalvageWalker")),
			TEXT("A_SalvageWalkerPrimaryAssetId"));
		Expect(IsValid(TurretDef) && TurretDef->GetPrimaryAssetId().PrimaryAssetName
				== FName(TEXT("DA_GP_Unit_DefensiveTurret")),
			TEXT("A_TurretPrimaryAssetId"));
		Expect(IsValid(WorkerDef) && WorkerDef->GetPrimaryAssetId().PrimaryAssetName
				== FName(TEXT("DA_GP_Unit_Worker")),
			TEXT("A_WorkerPrimaryAssetId"));

		WalkerWeak = GPUnitDefinitionContractDebug::SpawnWithDefinition<AGP_SalvageWalker>(
			World, GPUnitDefinitionContractDebug::Origin, WalkerDef);
		if (!Expect(IsValid(WalkerWeak.Get()) && WalkerWeak->ResolveLoadedUnitDefinition() == WalkerDef,
			TEXT("A_SpawnWalkerWithDefinition")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		const UGP_UnitAttributeSet* Attrs = Walker != nullptr ? Walker->GetUnitAttributeSet() : nullptr;
		Expect(Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 200.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetHealth(), 200.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 20.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetArmor(), 0.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamageResistance(), 0.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackCooldown(), 1.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 600.0f, 0.01f),
			TEXT("B_SalvageWalkerGasFromDefinition"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		Expect(Cmd != nullptr && FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 900.0f, 0.01f),
			TEXT("C_SightRangeOnCommandComponent"));
		Expect(Cmd != nullptr && FMath::IsNearlyEqual(Cmd->AutoAcquireScanIntervalSeconds, 0.35f, 0.01f),
			TEXT("D_ScanIntervalOnCommandComponent"));
		Expect(Cmd != nullptr
			&& FMath::IsNearlyEqual(Cmd->AttackFacingRotationSpeedDegreesPerSecond, 360.0f, 0.01f),
			TEXT("E_FacingSpeedOnCommandComponent"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_MovementComponent* Movement = Walker != nullptr ? Walker->GetUnitMovementComponent() : nullptr;
		Expect(Movement != nullptr && FMath::IsNearlyEqual(Movement->MoveSpeed, 250.0f, 0.01f),
			TEXT("F_MoveSpeedOnMovementComponent"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4:
	{
		TurretWeak = GPUnitDefinitionContractDebug::SpawnWithDefinition<AGP_DefensiveTurret>(
			World, GPUnitDefinitionContractDebug::Origin + FVector(0.0f, 400.0f, 0.0f), TurretDef);
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		const UGP_UnitAttributeSet* Attrs = Turret != nullptr ? Turret->GetUnitAttributeSet() : nullptr;
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		Expect(IsValid(Turret) && Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 400.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetHealth(), 400.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 20.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 600.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackCooldown(), 1.0f, 0.01f)
			&& Cmd != nullptr
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 600.0f, 0.01f)
			&& !Turret->IsA(AGP_MobileUnit::StaticClass()),
			TEXT("G_TurretCombatNoMovementDependency"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 5:
	{
		WorkerWeak = GPUnitDefinitionContractDebug::SpawnWithDefinition<AGP_Worker>(
			World, GPUnitDefinitionContractDebug::Origin + FVector(0.0f, -400.0f, 0.0f), WorkerDef);
		AGP_Worker* Worker = WorkerWeak.Get();
		const UGP_UnitAttributeSet* Attrs = Worker != nullptr ? Worker->GetUnitAttributeSet() : nullptr;
		UGP_MovementComponent* Movement = Worker != nullptr ? Worker->GetUnitMovementComponent() : nullptr;
		Expect(IsValid(Worker) && Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 100.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetHealth(), 100.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 25.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 250.0f, 0.01f)
			&& Movement != nullptr
			&& FMath::IsNearlyEqual(Movement->MoveSpeed, 600.0f, 0.01f),
			TEXT("H_WorkerExistingStats"));
		++StageIndex;
		ScheduleNext(0.4f);
		break;
	}
	case 6:
	{
		AGP_SalvageWalker* Walker = WalkerWeak.Get();
		UGP_UnitCommandComponent* Cmd = Walker != nullptr ? Walker->GetUnitCommandComponent() : nullptr;
		Expect(IsValid(WalkerDef)
			&& FMath::IsNearlyEqual(WalkerDef->RetaliationPursuitSeconds, 5.0f, 0.01f)
			&& Walker != nullptr
			&& FMath::IsNearlyEqual(Walker->GetRetaliationPursuitSeconds(), 5.0f, 0.01f)
			&& (Cmd == nullptr || !Cmd->IsAttackActive()),
			TEXT("I_RetaliationDataOnlyNoSideEffects"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7:
	{
		FallbackWalkerWeak = GPUnitDefinitionContractDebug::SpawnWithDefinition<AGP_SalvageWalker>(
			World, GPUnitDefinitionContractDebug::Origin + FVector(400.0f, 0.0f, 0.0f), nullptr);
		AGP_SalvageWalker* Fallback = FallbackWalkerWeak.Get();
		const UGP_UnitAttributeSet* Attrs = Fallback != nullptr ? Fallback->GetUnitAttributeSet() : nullptr;
		UGP_UnitCommandComponent* Cmd = Fallback != nullptr ? Fallback->GetUnitCommandComponent() : nullptr;
		UGP_MovementComponent* Movement = Fallback != nullptr ? Fallback->GetUnitMovementComponent() : nullptr;
		Expect(IsValid(Fallback) && Fallback->ResolveLoadedUnitDefinition() == nullptr
			&& Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 200.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 20.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 600.0f, 0.01f)
			&& Cmd != nullptr
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 900.0f, 0.01f)
			&& Movement != nullptr
			&& FMath::IsNearlyEqual(Movement->MoveSpeed, 250.0f, 0.01f)
			&& FMath::IsNearlyEqual(Fallback->GetRetaliationPursuitSeconds(),
				AGP_UnitBase::FallbackRetaliationPursuitSeconds, 0.01f),
			TEXT("J_EmptyDefinitionLegacyFallback"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8:
	{
		OverrideDef = NewObject<UGP_UnitDefinition>(this);
		OverrideDef->DisplayName = FText::FromString(TEXT("OverrideWalker"));
		OverrideDef->MaxHealth = 180.0f;
		OverrideDef->InitialHealth = 180.0f;
		OverrideDef->Damage = 11.0f;
		OverrideDef->AttackRangeCm = 333.0f;
		OverrideDef->AttackCooldownSeconds = 2.5f;
		OverrideDef->SightRangeCm = 777.0f;
		OverrideDef->AutoAcquireScanIntervalSeconds = 0.2f;
		OverrideDef->AttackFacingRotationSpeedDegreesPerSecond = 90.0f;
		OverrideDef->MoveSpeedCmPerSecond = 111.0f;
		OverrideDef->RetaliationPursuitSeconds = 5.0f;

		OverrideWalkerWeak = GPUnitDefinitionContractDebug::SpawnWithDefinition<AGP_SalvageWalker>(
			World, GPUnitDefinitionContractDebug::Origin + FVector(-400.0f, 0.0f, 0.0f), OverrideDef);
		AGP_SalvageWalker* Override = OverrideWalkerWeak.Get();
		const UGP_UnitAttributeSet* Attrs = Override != nullptr ? Override->GetUnitAttributeSet() : nullptr;
		UGP_UnitCommandComponent* Cmd = Override != nullptr ? Override->GetUnitCommandComponent() : nullptr;
		UGP_MovementComponent* Movement = Override != nullptr ? Override->GetUnitMovementComponent() : nullptr;
		Expect(IsValid(Override) && Attrs != nullptr && Cmd != nullptr && Movement != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 180.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 11.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 333.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackCooldown(), 2.5f, 0.01f)
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 777.0f, 0.01f)
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireScanIntervalSeconds, 0.2f, 0.01f)
			&& FMath::IsNearlyEqual(Cmd->AttackFacingRotationSpeedDegreesPerSecond, 90.0f, 0.01f)
			&& FMath::IsNearlyEqual(Movement->MoveSpeed, 111.0f, 0.01f),
			TEXT("K_DefinitionOverridesLegacyDefaults"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9:
	{
		UGP_BuildingDefinition* BuildingDef = UGP_BuildingDropCatalog::Get().FindBuildingDefinition(
			FPrimaryAssetId(
				FPrimaryAssetType(UGP_BuildingDefinition::PrimaryAssetTypeName()),
				FName(TEXT("DA_GP_Building_DefensiveTurret"))));
		const UGP_UnitDefinition* Linked = BuildingDef != nullptr
			? BuildingDef->ResolveLoadedUnitDefinition()
			: nullptr;
		Expect(IsValid(BuildingDef)
			&& Linked == TurretDef
			&& FMath::IsNearlyEqual(BuildingDef->ResolveCanonicalMaxHealth(), 400.0f, 0.01f)
			&& FMath::IsNearlyEqual(Linked->MaxHealth, 400.0f, 0.01f)
			&& BuildingDef->MaxHealth == 400.0f,
			TEXT("L_BuildingDefinitionCanonicalMaxHealth"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10:
	{
		AsyncDef = NewObject<UGP_UnitDefinition>(this);
		AsyncDef->DisplayName = FText::FromString(TEXT("AsyncWalker"));
		AsyncDef->MaxHealth = 180.0f;
		AsyncDef->InitialHealth = 180.0f;
		AsyncDef->Damage = 11.0f;
		AsyncDef->AttackRangeCm = 333.0f;
		AsyncDef->AttackCooldownSeconds = 2.5f;
		AsyncDef->SightRangeCm = 777.0f;
		AsyncDef->AutoAcquireScanIntervalSeconds = 0.2f;
		AsyncDef->AttackFacingRotationSpeedDegreesPerSecond = 90.0f;
		AsyncDef->MoveSpeedCmPerSecond = 111.0f;
		AsyncDef->RetaliationPursuitSeconds = 5.0f;

		AsyncWalkerWeak = GPUnitDefinitionContractDebug::SpawnWithForcedAsync<AGP_SalvageWalker>(
			World,
			GPUnitDefinitionContractDebug::Origin + FVector(0.0f, 800.0f, 0.0f),
			AsyncDef,
			true);
		AGP_SalvageWalker* AsyncWalker = AsyncWalkerWeak.Get();
		const UGP_UnitAttributeSet* Attrs = AsyncWalker != nullptr ? AsyncWalker->GetUnitAttributeSet() : nullptr;
		UGP_UnitCommandComponent* Cmd = AsyncWalker != nullptr ? AsyncWalker->GetUnitCommandComponent() : nullptr;
		UGP_MovementComponent* Movement = AsyncWalker != nullptr ? AsyncWalker->GetUnitMovementComponent() : nullptr;
		Expect(IsValid(AsyncWalker)
			&& AsyncWalker->DebugDidRequestAsyncUnitDefinitionLoad()
			&& AsyncWalker->IsUnitDefinitionLoadPending()
			&& !AsyncWalker->IsUnitDefinitionReady()
			&& Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 0.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 0.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 0.0f, 0.01f)
			&& Cmd != nullptr
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 900.0f, 0.01f)
			&& Movement != nullptr
			&& FMath::IsNearlyEqual(Movement->MoveSpeed, 250.0f, 0.01f),
			TEXT("C_D_UnresolvedSoftRefDefersGasAndTuning"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11:
	{
		AGP_SalvageWalker* AsyncWalker = AsyncWalkerWeak.Get();
		if (!Expect(IsValid(AsyncWalker), TEXT("C_AsyncWalkerStillAlive")))
		{
			Finish();
			return;
		}
		AsyncWalker->DebugCompletePendingUnitDefinitionLoad();
		const UGP_UnitAttributeSet* Attrs = AsyncWalker->GetUnitAttributeSet();
		UGP_UnitCommandComponent* Cmd = AsyncWalker->GetUnitCommandComponent();
		UGP_MovementComponent* Movement = AsyncWalker->GetUnitMovementComponent();
		Expect(AsyncWalker->IsUnitDefinitionReady()
			&& !AsyncWalker->IsUnitDefinitionLoadPending()
			&& Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 180.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 11.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 333.0f, 0.01f)
			&& Cmd != nullptr
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireSightRangeCm, 777.0f, 0.01f)
			&& FMath::IsNearlyEqual(Cmd->AutoAcquireScanIntervalSeconds, 0.2f, 0.01f)
			&& FMath::IsNearlyEqual(Cmd->AttackFacingRotationSpeedDegreesPerSecond, 90.0f, 0.01f)
			&& Movement != nullptr
			&& FMath::IsNearlyEqual(Movement->MoveSpeed, 111.0f, 0.01f)
			&& FMath::IsNearlyEqual(AsyncWalker->GetRetaliationPursuitSeconds(), 5.0f, 0.01f),
			TEXT("E_F_AsyncCompletionAppliesDefinition"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12:
	{
		FailWalkerWeak = GPUnitDefinitionContractDebug::SpawnWithForcedAsync<AGP_SalvageWalker>(
			World,
			GPUnitDefinitionContractDebug::Origin + FVector(800.0f, 0.0f, 0.0f),
			nullptr,
			false);
		FailWaitTicks = 0;
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 13:
	{
		AGP_SalvageWalker* FailWalker = FailWalkerWeak.Get();
		if (IsValid(FailWalker) && !FailWalker->IsUnitDefinitionReady() && FailWaitTicks < 40)
		{
			++FailWaitTicks;
			ScheduleNext(0.05f);
			return;
		}
		const UGP_UnitAttributeSet* Attrs = FailWalker != nullptr ? FailWalker->GetUnitAttributeSet() : nullptr;
		Expect(IsValid(FailWalker)
			&& FailWalker->DebugDidRequestAsyncUnitDefinitionLoad()
			&& FailWalker->IsUnitDefinitionReady()
			&& Attrs != nullptr
			&& FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 200.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetDamage(), 20.0f, 0.01f)
			&& FMath::IsNearlyEqual(Attrs->GetAttackRange(), 600.0f, 0.01f)
			&& FMath::IsNearlyEqual(FailWalker->GetRetaliationPursuitSeconds(),
				AGP_UnitBase::FallbackRetaliationPursuitSeconds, 0.01f),
			TEXT("G_LoadFailureDeterministicFallback"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 14:
	{
		AGP_SalvageWalker* Pending = GPUnitDefinitionContractDebug::SpawnWithForcedAsync<AGP_SalvageWalker>(
			World,
			GPUnitDefinitionContractDebug::Origin + FVector(-800.0f, 0.0f, 0.0f),
			AsyncDef,
			true);
		const bool bPending = IsValid(Pending)
			&& Pending->DebugDidRequestAsyncUnitDefinitionLoad()
			&& !Pending->IsUnitDefinitionReady();
		if (IsValid(Pending))
		{
			Pending->Destroy();
		}
		Expect(bPending && !IsValid(Pending), TEXT("H_EndPlayPendingLoadSafe"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else // UE_BUILD_SHIPPING

void UGP_UnitDefinitionContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_UnitDefinitionContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_UnitDefinitionContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_UnitDefinitionContractTestRunner::AdvanceStage() {}
bool UGP_UnitDefinitionContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_UnitDefinitionContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_UnitDefinitionContractTestRunner::Finish() { bFinished = true; }
void UGP_UnitDefinitionContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_UnitDefinitionContractTestRunner::UnbindWorldCleanup() {}
void UGP_UnitDefinitionContractTestRunner::CleanupActors() {}

#endif
