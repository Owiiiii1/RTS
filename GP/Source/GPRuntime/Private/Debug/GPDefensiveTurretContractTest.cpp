// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPDefensiveTurretContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPMainBase.h"
#include "Orbital/GPBuildGridContractTest.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Combat/GPCombatLOS.h"
#include "Combat/GPDamageApplication.h"
#include "Command/GPUnitCommand.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Effects/GPGE_DamageBasic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#include "Units/GPMobileUnit.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPDefensiveTurretContract, Log, All);

namespace GPDefensiveTurretContractDebug
{
	static TWeakObjectPtr<UGP_DefensiveTurretContractTestRunner> GActiveRunner;
	static constexpr int32 ContractTeam = 1;
	static constexpr int32 EnemyTeam = 2;
	static const FVector TurretLoc(-52000.0f, 0.0f, 100.0f);
	static const FVector FriendlyLoc(-51650.0f, 0.0f, 100.0f);
	static const FVector FarEnemyLoc(-50500.0f, 0.0f, 100.0f);
	static const FVector NearEnemyLoc(-51600.0f, 200.0f, 100.0f);
	static const FVector ReacquireLoc(-51620.0f, -180.0f, 100.0f);
	static const FVector FriendlyBuildingLoc(-51650.0f, -220.0f, 100.0f);
	static const FVector DeadBuildingLoc(-51600.0f, 280.0f, 100.0f);
	static const FVector EnemyBuildingLoc(-51600.0f, 200.0f, 100.0f);
	static const FVector ReacquireBuildingLoc(-51620.0f, -180.0f, 100.0f);
	static const FVector MainBaseLoc(-50000.0f, 4000.0f, 100.0f);

	static void ApplyCombatStats(AGP_UnitBase* Unit, float Health, float Damage, float Range, float Cooldown)
	{
		if (Unit == nullptr)
		{
			return;
		}
		UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return;
		}
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetMaxHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), Damage);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackRangeAttribute(), Range);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackCooldownAttribute(), Cooldown);
	}

	static float GetHealth(const AGP_UnitBase* Unit)
	{
		const UGP_UnitAttributeSet* Attrs = Unit != nullptr ? Unit->GetUnitAttributeSet() : nullptr;
		return Attrs != nullptr ? Attrs->GetHealth() : -1.0f;
	}

	static AGP_DefensiveTurret* SpawnTurret(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_DefensiveTurret* Turret = World->SpawnActor<AGP_DefensiveTurret>(
			AGP_DefensiveTurret::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Turret != nullptr)
		{
			Turret->SetTeamId(TeamId);
			if (UGP_UnitCommandComponent* Cmd = Turret->GetUnitCommandComponent())
			{
				Cmd->AutoAcquireScanIntervalSeconds = 0.05f;
				Cmd->RefreshCombatAutoAcquireTimer();
			}
		}
		return Turret;
	}

	static AGP_BuildingBase* SpawnBuildingStub(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_BuildingBase* Building = World->SpawnActor<AGP_BuildGridContractStub>(
			AGP_BuildGridContractStub::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Building != nullptr)
		{
			Building->SetActorLocation(Loc);
			Building->SetTeamId(TeamId);
			ApplyCombatStats(Building, 200.0f, 0.0f, 0.0f, 1.0f);
			if (USceneComponent* Root = Building->GetRootComponent())
			{
				UCapsuleComponent* Visibility = NewObject<UCapsuleComponent>(Building, TEXT("ContractVisibility"));
				Visibility->InitCapsuleSize(60.0f, 100.0f);
				Visibility->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Visibility->SetCollisionObjectType(ECC_WorldDynamic);
				Visibility->SetCollisionResponseToAllChannels(ECR_Ignore);
				Visibility->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
				Visibility->SetGenerateOverlapEvents(false);
				Visibility->SetCanEverAffectNavigation(false);
				Visibility->SetupAttachment(Root);
				Visibility->RegisterComponent();
			}
		}
		return Building;
	}

	static AGP_Worker* SpawnWorker(UWorld* World, const FVector& Loc, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Worker != nullptr)
		{
			Worker->SetTeamId(TeamId);
			ApplyCombatStats(Worker, 200.0f, 0.0f, 0.0f, 1.0f);
		}
		return Worker;
	}

	static AActor* SpawnVisibilityBlocker(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (Blocker == nullptr)
		{
			return nullptr;
		}

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Blocker, TEXT("BlockerMesh"));
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->RegisterComponent();
		Blocker->SetRootComponent(Mesh);
		Mesh->SetWorldLocation(Loc);
		Mesh->SetWorldScale3D(FVector(4.0f, 4.0f, 8.0f));
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(CubeMesh);
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		return Blocker;
	}

	static bool KillUnit(AGP_UnitBase* Unit)
	{
		if (!IsValid(Unit))
		{
			return false;
		}
		UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		UGP_UnitAttributeSet* Attr = const_cast<UGP_UnitAttributeSet*>(Unit->GetUnitAttributeSet());
		if (ASC == nullptr || Attr == nullptr)
		{
			return false;
		}
		Attr->SetDamage(FMath::Max(Attr->GetMaxHealth(), 1.0f) + 1000.0f);
		FGP_DamageApplicationResult Result;
		GPDamageApplication::ApplyDamageEffect(ASC, ASC, UGP_GE_Damage_Basic::StaticClass(), Result);
		return Unit->IsDead();
	}

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

	static float GetOrbital(const AGP_PlayerState* PS)
	{
		const UGP_PlayerAttributeSet* Attr = PS != nullptr ? PS->GetPlayerAttributeSet() : nullptr;
		return Attr != nullptr ? Attr->GetOrbitalFerronite() : -1.0f;
	}

	static UGP_OrbitalDropDefinition* FindTurretDrop()
	{
		const FPrimaryAssetId Id(
			FPrimaryAssetType(UGP_OrbitalDropDefinition::PrimaryAssetTypeName()),
			FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret")));
		return UGP_BuildingDropCatalog::Get().FindDropDefinition(Id);
	}

	static void RunDefensiveTurretContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPDefensiveTurretContract, Warning,
				TEXT("gp.Building.RunDefensiveTurretContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPDefensiveTurretContract, Warning,
				TEXT("gp.Building.RunDefensiveTurretContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("DefensiveTurretContract"), TEXT("DefensiveTurret"), Token))
		{
			return;
		}

		UGP_DefensiveTurretContractTestRunner* Runner =
			NewObject<UGP_DefensiveTurretContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GDefensiveTurretContract(
		TEXT("gp.Building.RunDefensiveTurretContractTest"),
		TEXT("GP-S37T Defensive Turret combat + orbital contract (A–O + purchase/deploy)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunDefensiveTurretContractTest));
}

void UGP_DefensiveTurretContractTestRunner::BeginDestroy()
{
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_DefensiveTurretContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_DefensiveTurretContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_DefensiveTurretContractTestRunner::RestoreSettings()
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
		Settings->BuildingMaxDeployRadiusFromMainBaseCm = SavedBuildingMaxRadius;
	}
	bSettingsMutated = false;
}

void UGP_DefensiveTurretContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}
		if (AActor* Blocker = BlockerWeak.Get())
		{
			Blocker->Destroy();
		}
		if (AGP_Worker* Worker = FriendlyWeak.Get())
		{
			Worker->Destroy();
		}
		if (AGP_Worker* Worker = FarEnemyWeak.Get())
		{
			Worker->Destroy();
		}
		if (AGP_Worker* Worker = NearEnemyWeak.Get())
		{
			Worker->Destroy();
		}
		if (AGP_Worker* Worker = ReacquireEnemyWeak.Get())
		{
			Worker->Destroy();
		}
		if (AGP_BuildingBase* Building = FriendlyBuildingWeak.Get())
		{
			Building->Destroy();
		}
		if (AGP_BuildingBase* Building = DeadEnemyBuildingWeak.Get())
		{
			Building->Destroy();
		}
		if (AGP_BuildingBase* Building = EnemyBuildingWeak.Get())
		{
			Building->Destroy();
		}
		if (AGP_BuildingBase* Building = ReacquireBuildingWeak.Get())
		{
			Building->Destroy();
		}
		if (AGP_DefensiveTurret* Turret = TurretWeak.Get())
		{
			Turret->Destroy();
		}
		if (AGP_DefensiveTurret* Turret = OrbitalTurretWeak.Get())
		{
			Turret->Destroy();
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
	TurretWeak.Reset();
	FriendlyWeak.Reset();
	FarEnemyWeak.Reset();
	NearEnemyWeak.Reset();
	ReacquireEnemyWeak.Reset();
	FriendlyBuildingWeak.Reset();
	DeadEnemyBuildingWeak.Reset();
	EnemyBuildingWeak.Reset();
	ReacquireBuildingWeak.Reset();
	BlockerWeak.Reset();
	MainBaseWeak.Reset();
	OwnerPSWeak.Reset();
	LastPodWeak.Reset();
	OrbitalTurretWeak.Reset();
}

void UGP_DefensiveTurretContractTestRunner::Finish()
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
	UE_LOG(LogGPDefensiveTurretContract, Log,
		TEXT("gp.Building.RunDefensiveTurretContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPDefensiveTurretContractDebug::GActiveRunner.Reset();
}

void UGP_DefensiveTurretContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	bCancelled = true;
	CancelReason = FName(Reason);
	UE_LOG(LogGPDefensiveTurretContract, Error, TEXT("gp.Building.RunDefensiveTurretContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_DefensiveTurretContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (bOk)
	{
		UE_LOG(LogGPDefensiveTurretContract, Log, TEXT("gp.Building.RunDefensiveTurretContractTest PASS: %s"), Label);
		return true;
	}
	++Failures;
	UE_LOG(LogGPDefensiveTurretContract, Error, TEXT("gp.Building.RunDefensiveTurretContractTest FAIL: %s"), Label);
	return false;
}

void UGP_DefensiveTurretContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		this,
		&UGP_DefensiveTurretContractTestRunner::AdvanceStage,
		DelaySeconds,
		false);
}

void UGP_DefensiveTurretContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_DefensiveTurretContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPDefensiveTurretContract, Log, TEXT("gp.Building.RunDefensiveTurretContractTest Start"));
	AdvanceStage();
}

void UGP_DefensiveTurretContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();

	switch (StageIndex)
	{
	case 0:
	{
		AGP_DefensiveTurret* Turret = GPDefensiveTurretContractDebug::SpawnTurret(
			World, GPDefensiveTurretContractDebug::TurretLoc, GPDefensiveTurretContractDebug::ContractTeam);
		TurretWeak = Turret;
		if (!Expect(IsValid(Turret), TEXT("A_NativeSpawn")))
		{
			Finish();
			return;
		}

		Expect(Turret->HasCapabilityTag(GPTags.Capability_Selectable)
			&& Turret->HasCapabilityTag(GPTags.Capability_Inspectable)
			&& Turret->HasCapabilityTag(GPTags.Selection_Type_Building)
			&& Turret->HasCapabilityTag(GPTags.Unit_Type_Building)
			&& Turret->HasCapabilityTag(GPTags.Building_Type_DefensiveTurret),
			TEXT("B_IdentityTags"));

		UGP_UnitCommandComponent* Cmd = Turret->GetUnitCommandComponent();
		Expect(!Turret->IsA(AGP_MobileUnit::StaticClass())
			&& Cmd != nullptr
			&& !Cmd->IsEligibleForAttackMoveAcquire()
			&& !Cmd->IsAttackMoveActive(),
			TEXT("C_StationaryNoMoveOrAttackMove"));
		if (Cmd != nullptr)
		{
			FGP_UnitCommand MoveCmd;
			MoveCmd.CommandTag = GPTags.Command_Move;
			MoveCmd.TargetLocation = Turret->GetActorLocation() + FVector(400.0f, 0.0f, 0.0f);
			Cmd->HandleCommand(MoveCmd);
			Expect(!Cmd->IsAttackMoveActive() && !Cmd->IsAttackActive(), TEXT("C_MoveRejected"));
		}

		const UGP_UnitAttributeSet* Attrs = Turret->GetUnitAttributeSet();
		Expect(Attrs != nullptr && Attrs->GetHealth() > 0.0f && Attrs->GetMaxHealth() > 0.0f,
			TEXT("D_HealthInitialized"));

		++StageIndex;
		ScheduleNext(0.1f);
		break;
	}
	case 1:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
		if (!Expect(IsValid(Turret) && Grid != nullptr, TEXT("N_TurretAndGrid")))
		{
			Finish();
			return;
		}
		OccupantId = Turret->GetGridOccupantId();
		OccupiedCells.Reset();
		const bool bHasCells = Grid->GetOccupantCells(OccupantId, OccupiedCells);
		Expect(bHasCells && OccupiedCells.Num() == 4, TEXT("N_BuildGridOccupies2x2"));

		FriendlyWeak = GPDefensiveTurretContractDebug::SpawnWorker(
			World, GPDefensiveTurretContractDebug::FriendlyLoc, GPDefensiveTurretContractDebug::ContractTeam);
		if (!Expect(IsValid(FriendlyWeak.Get()), TEXT("E_SpawnFriendly")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.45f);
		break;
	}
	case 2:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_Worker* Friendly = FriendlyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		const float FriendlyHp = GPDefensiveTurretContractDebug::GetHealth(Friendly);
		Expect(IsValid(Friendly) && Cmd != nullptr && !Cmd->IsAttackActive() && FriendlyHp >= 199.0f,
			TEXT("E_FriendlyNotAcquired"));

		FarEnemyWeak = GPDefensiveTurretContractDebug::SpawnWorker(
			World, GPDefensiveTurretContractDebug::FarEnemyLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (!Expect(IsValid(FarEnemyWeak.Get()), TEXT("F_SpawnFarEnemy")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.45f);
		break;
	}
	case 3:
	{
		AGP_Worker* Far = FarEnemyWeak.Get();
		Expect(IsValid(Far) && GPDefensiveTurretContractDebug::GetHealth(Far) >= 199.0f,
			TEXT("F_EnemyOutsideRangeNoDamage"));

		NearEnemyWeak = GPDefensiveTurretContractDebug::SpawnWorker(
			World, GPDefensiveTurretContractDebug::NearEnemyLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (!Expect(IsValid(NearEnemyWeak.Get()), TEXT("G_SpawnNearEnemy")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.85f);
		break;
	}
	case 4:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_Worker* Near = NearEnemyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		const float NearHp = GPDefensiveTurretContractDebug::GetHealth(Near);
		const bool bLos = Turret != nullptr && Near != nullptr
			&& GPCombatLOS::HasLineOfSight(World, Turret, Near);
		Expect(IsValid(Near) && Cmd != nullptr && Cmd->IsAttackActive() && bLos, TEXT("G_EnemyInRangeAcquired"));
		Expect(NearHp > 0.0f && NearHp < 200.0f, TEXT("I_DamageViaExistingPath"));
		NearHpAfterFirstHit = NearHp;
		const double Now = World->GetTimeSeconds();
		const double NextHit = Cmd != nullptr ? Cmd->GetNextAttackHitTime() : Now;
		const float CooldownProbe = FMath::Clamp(static_cast<float>(NextHit - Now) * 0.4f, 0.12f, 0.45f);
		++StageIndex;
		ScheduleNext(CooldownProbe);
		break;
	}
	case 5:
	{
		AGP_Worker* Near = NearEnemyWeak.Get();
		const float NearHp = GPDefensiveTurretContractDebug::GetHealth(Near);
		Expect(IsValid(Near) && FMath::IsNearlyEqual(NearHp, NearHpAfterFirstHit, 0.5f),
			TEXT("H_NoPerFrameDamage"));
		NearHpAfterCooldownWindow = NearHp;
		++StageIndex;
		ScheduleNext(1.05f);
		break;
	}
	case 6:
	{
		AGP_Worker* Near = NearEnemyWeak.Get();
		const float NearHp = GPDefensiveTurretContractDebug::GetHealth(Near);
		Expect(IsValid(Near) && NearHp < NearHpAfterCooldownWindow - 0.5f, TEXT("H_CooldownAllowsNextHit"));
		if (IsValid(Near))
		{
			GPDefensiveTurretContractDebug::KillUnit(Near);
		}
		Expect(!IsValid(Near) || Near->IsDead(), TEXT("J_EnemyDied"));
		ReacquireEnemyWeak = GPDefensiveTurretContractDebug::SpawnWorker(
			World, GPDefensiveTurretContractDebug::ReacquireLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (!Expect(IsValid(ReacquireEnemyWeak.Get()), TEXT("J_SpawnReacquireEnemy")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.85f);
		break;
	}
	case 7:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_Worker* Next = ReacquireEnemyWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		const float NextHp = GPDefensiveTurretContractDebug::GetHealth(Next);
		Expect(IsValid(Next) && Cmd != nullptr && Cmd->IsAttackActive() && NextHp < 200.0f,
			TEXT("J_ReacquiredAfterDeath"));

		const FVector BlockLoc = (GPDefensiveTurretContractDebug::TurretLoc + Next->GetActorLocation()) * 0.5f
			+ FVector(0.0f, 0.0f, 80.0f);
		BlockerWeak = GPDefensiveTurretContractDebug::SpawnVisibilityBlocker(World, BlockLoc);
		if (!Expect(IsValid(BlockerWeak.Get()), TEXT("K_SpawnBlocker")))
		{
			Finish();
			return;
		}
		ReacquireHpAtBlock = GPDefensiveTurretContractDebug::GetHealth(Next);
		++StageIndex;
		ScheduleNext(1.15f);
		break;
	}
	case 8:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_Worker* Next = ReacquireEnemyWeak.Get();
		const bool bBlocked = Turret != nullptr && Next != nullptr
			&& !GPCombatLOS::HasLineOfSight(World, Turret, Next);
		const float NextHp = GPDefensiveTurretContractDebug::GetHealth(Next);
		Expect(bBlocked && FMath::IsNearlyEqual(NextHp, ReacquireHpAtBlock, 0.5f), TEXT("K_LOSBlockedNoDamage"));
		if (AActor* Blocker = BlockerWeak.Get())
		{
			Blocker->Destroy();
			BlockerWeak.Reset();
		}
		++StageIndex;
		ScheduleNext(1.15f);
		break;
	}
	case 9:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_Worker* Next = ReacquireEnemyWeak.Get();
		const bool bClear = Turret != nullptr && Next != nullptr
			&& GPCombatLOS::HasLineOfSight(World, Turret, Next);
		const float NextHp = GPDefensiveTurretContractDebug::GetHealth(Next);
		Expect(bClear && NextHp < ReacquireHpAtBlock - 0.5f, TEXT("L_LOSRestoredCanAttack"));

		if (AGP_Worker* Worker = FriendlyWeak.Get())
		{
			Worker->Destroy();
			FriendlyWeak.Reset();
		}
		if (AGP_Worker* Worker = FarEnemyWeak.Get())
		{
			Worker->Destroy();
			FarEnemyWeak.Reset();
		}
		if (AGP_Worker* Worker = NearEnemyWeak.Get())
		{
			Worker->Destroy();
			NearEnemyWeak.Reset();
		}
		if (IsValid(Next))
		{
			Next->Destroy();
			ReacquireEnemyWeak.Reset();
		}
		if (UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr)
		{
			FGP_UnitCommand StopCmd;
			StopCmd.CommandTag = GPTags.Command_Stop;
			Cmd->HandleCommand(StopCmd);
		}

		FriendlyBuildingWeak = GPDefensiveTurretContractDebug::SpawnBuildingStub(
			World, GPDefensiveTurretContractDebug::FriendlyBuildingLoc, GPDefensiveTurretContractDebug::ContractTeam);
		DeadEnemyBuildingWeak = GPDefensiveTurretContractDebug::SpawnBuildingStub(
			World, GPDefensiveTurretContractDebug::DeadBuildingLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (AGP_BuildingBase* DeadBuilding = DeadEnemyBuildingWeak.Get())
		{
			GPDefensiveTurretContractDebug::KillUnit(DeadBuilding);
		}
		if (!Expect(IsValid(FriendlyBuildingWeak.Get())
			&& IsValid(DeadEnemyBuildingWeak.Get())
			&& DeadEnemyBuildingWeak->IsDead(),
			TEXT("Building_SpawnFriendlyAndDead")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.5f);
		break;
	}
	case 10:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		AGP_BuildingBase* FriendlyBuilding = FriendlyBuildingWeak.Get();
		AGP_BuildingBase* DeadBuilding = DeadEnemyBuildingWeak.Get();
		Expect(Cmd != nullptr && !Cmd->IsAttackActive()
			&& GPDefensiveTurretContractDebug::GetHealth(FriendlyBuilding) >= 199.0f,
			TEXT("C_FriendlyBuildingNotAcquired"));
		Expect(IsValid(DeadBuilding) && DeadBuilding->IsDead()
			&& (Cmd == nullptr || Cmd->GetAttackTarget() != DeadBuilding),
			TEXT("D_DeadEnemyBuildingNotAcquired"));

		EnemyBuildingWeak = GPDefensiveTurretContractDebug::SpawnBuildingStub(
			World, GPDefensiveTurretContractDebug::EnemyBuildingLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (!Expect(IsValid(EnemyBuildingWeak.Get())
			&& FVector::Dist2D(EnemyBuildingWeak->GetActorLocation(), GPDefensiveTurretContractDebug::EnemyBuildingLoc) < 50.0f,
			TEXT("B_SpawnEnemyBuilding")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.85f);
		break;
	}
	case 11:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_BuildingBase* EnemyBuilding = EnemyBuildingWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		const float BuildingHp = GPDefensiveTurretContractDebug::GetHealth(EnemyBuilding);
		const bool bLos = Turret != nullptr && EnemyBuilding != nullptr
			&& GPCombatLOS::HasLineOfSight(World, Turret, EnemyBuilding);
		if (!Expect(IsValid(EnemyBuilding) && Cmd != nullptr && Cmd->IsAttackActive()
			&& Cmd->GetAttackTarget() == EnemyBuilding && bLos && BuildingHp < 200.0f,
			TEXT("B_EnemyBuildingAcquiredAndDamaged")))
		{
			Finish();
			return;
		}
		EnemyBuildingHpAtAcquire = BuildingHp;

		const FVector BlockLoc = (GPDefensiveTurretContractDebug::TurretLoc + EnemyBuilding->GetActorLocation()) * 0.5f
			+ FVector(0.0f, 0.0f, 80.0f);
		BlockerWeak = GPDefensiveTurretContractDebug::SpawnVisibilityBlocker(World, BlockLoc);
		if (!Expect(IsValid(BlockerWeak.Get()), TEXT("E_SpawnBuildingBlocker")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(1.15f);
		break;
	}
	case 12:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_BuildingBase* EnemyBuilding = EnemyBuildingWeak.Get();
		const bool bBlocked = Turret != nullptr && EnemyBuilding != nullptr
			&& !GPCombatLOS::HasLineOfSight(World, Turret, EnemyBuilding);
		const float BuildingHp = GPDefensiveTurretContractDebug::GetHealth(EnemyBuilding);
		Expect(bBlocked && FMath::IsNearlyEqual(BuildingHp, EnemyBuildingHpAtAcquire, 0.5f),
			TEXT("E_LOSBlockedBuildingNoDamage"));
		if (AActor* Blocker = BlockerWeak.Get())
		{
			Blocker->Destroy();
			BlockerWeak.Reset();
		}
		if (IsValid(EnemyBuilding))
		{
			GPDefensiveTurretContractDebug::KillUnit(EnemyBuilding);
		}
		Expect(!IsValid(EnemyBuilding) || EnemyBuilding->IsDead(), TEXT("F_EnemyBuildingDied"));
		ReacquireBuildingWeak = GPDefensiveTurretContractDebug::SpawnBuildingStub(
			World, GPDefensiveTurretContractDebug::ReacquireBuildingLoc, GPDefensiveTurretContractDebug::EnemyTeam);
		if (!Expect(IsValid(ReacquireBuildingWeak.Get()), TEXT("F_SpawnReacquireBuilding")))
		{
			Finish();
			return;
		}
		++StageIndex;
		ScheduleNext(0.85f);
		break;
	}
	case 13:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		AGP_BuildingBase* ReacquireBuilding = ReacquireBuildingWeak.Get();
		UGP_UnitCommandComponent* Cmd = Turret != nullptr ? Turret->GetUnitCommandComponent() : nullptr;
		const float BuildingHp = GPDefensiveTurretContractDebug::GetHealth(ReacquireBuilding);
		Expect(IsValid(ReacquireBuilding) && Cmd != nullptr && Cmd->IsAttackActive()
			&& Cmd->GetAttackTarget() == ReacquireBuilding && BuildingHp < 200.0f,
			TEXT("F_ReacquiredBuildingAfterDeath"));
		if (IsValid(Turret))
		{
			GPDefensiveTurretContractDebug::KillUnit(Turret);
		}
		Expect(IsValid(Turret) && Turret->IsDead() && (Cmd == nullptr || !Cmd->IsAttackActive()),
			TEXT("M_TurretDeathStopsCombat"));
		++StageIndex;
		ScheduleNext(0.15f);
		break;
	}
	case 14:
	{
		AGP_DefensiveTurret* Turret = TurretWeak.Get();
		UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
		if (IsValid(Turret))
		{
			Turret->Destroy();
			TurretWeak.Reset();
		}
		TArray<FIntPoint> Remaining;
		const bool bStillRegistered = Grid != nullptr && OccupantId.IsValid()
			&& Grid->GetOccupantCells(OccupantId, Remaining) && Remaining.Num() > 0;
		bool bCellsFree = Grid != nullptr;
		if (Grid != nullptr)
		{
			for (const FIntPoint& Cell : OccupiedCells)
			{
				if (Grid->IsCellOccupied(Cell, nullptr))
				{
					bCellsFree = false;
					break;
				}
			}
		}
		Expect(!bStillRegistered && bCellsFree, TEXT("O_DestroyReleasesCells"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 15:
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedBuildingDescent = Settings->BuildingDropDescentDurationSeconds;
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			SavedBuildingDeployDelay = Settings->BuildingDropPayloadDeployDelaySeconds;
			SavedBuildingMaxRadius = Settings->BuildingMaxDeployRadiusFromMainBaseCm;
			Settings->BuildingDropDescentDurationSeconds = 0.05f;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.0f;
			Settings->BuildingMaxDeployRadiusFromMainBaseCm = 8000.0f;
			bSettingsMutated = true;
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* Base = GS != nullptr
			? GS->FindMainBaseForTeam(GPDefensiveTurretContractDebug::ContractTeam)
			: nullptr;
		if (!IsValid(Base))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			Base = World->SpawnActor<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				GPDefensiveTurretContractDebug::MainBaseLoc,
				FRotator::ZeroRotator,
				Params);
			if (IsValid(Base))
			{
				Base->SetTeamId(GPDefensiveTurretContractDebug::ContractTeam);
			}
			MainBaseWeak = Base;
		}
		if (!Expect(IsValid(Base), TEXT("Orbital_SpawnMainBase")))
		{
			Finish();
			return;
		}
		ValidDeployLocation = Base->GetActorLocation() + FVector(1400.0f, 0.0f, 0.0f);
		RejectDeployLocation = Base->GetActorLocation();

		AGP_PlayerState* OwnerPS = GPDefensiveTurretContractDebug::SpawnTeamPlayerState(
			World, GS, GPDefensiveTurretContractDebug::ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS), TEXT("Orbital_SpawnOwnerPS")))
		{
			Finish();
			return;
		}
		GPDefensiveTurretContractDebug::GrantOrbital(OwnerPS, 400.0f);
		OrbitalBeforePurchase = GPDefensiveTurretContractDebug::GetOrbital(OwnerPS);

		UGP_OrbitalDropDefinition* TurretDrop = GPDefensiveTurretContractDebug::FindTurretDrop();
		if (!Expect(IsValid(TurretDrop)
			&& UGP_BuildingDropCatalog::Get().ResolvePayloadClass(TurretDrop) != nullptr,
			TEXT("Orbital_TurretDropResolved")))
		{
			Finish();
			return;
		}

		const float Cost = UGP_BuildingDropCatalog::Get().GetPurchaseCost(TurretDrop);
		GPBuildingDropAuthority::FPurchaseResult Purchase =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, TurretDrop);
		const float OrbitalAfter = GPDefensiveTurretContractDebug::GetOrbital(OwnerPS);
		ReadyAfterPurchase = OwnerPS->GetOrbitalBuildingInventoryComponent() != nullptr
			? OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(TurretDrop)
			: -1;
		Expect(Purchase.bAccepted && FMath::IsNearlyEqual(Cost, 150.0f)
			&& FMath::IsNearlyEqual(OrbitalAfter, OrbitalBeforePurchase - Cost, 0.5f)
			&& ReadyAfterPurchase == 1,
			TEXT("Orbital_PurchaseSpendOnceReadyPlusOne"));

		OrbitalBeforeDeploy = OrbitalAfter;
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 16:
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* TurretDrop = GPDefensiveTurretContractDebug::FindTurretDrop();
		if (!Expect(IsValid(OwnerPS) && IsValid(TurretDrop), TEXT("Orbital_DeployPreconditions")))
		{
			Finish();
			return;
		}

		GPBuildingDropAuthority::FDeployResult Rejected =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				TurretDrop,
				FTransform(FRotator::ZeroRotator, RejectDeployLocation));
		const int32 ReadyAfterReject = OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(TurretDrop);
		const float OrbitalAfterReject = GPDefensiveTurretContractDebug::GetOrbital(OwnerPS);
		Expect(!Rejected.bAccepted
			&& ReadyAfterReject == ReadyAfterPurchase
			&& FMath::IsNearlyEqual(OrbitalAfterReject, OrbitalBeforeDeploy, 0.5f)
			&& !Rejected.SpawnedPod.IsValid(),
			TEXT("Orbital_RejectPreservesReadyAndSpend"));

		const FVector DeployOffsets[] = {
			FVector(1400.0f, 0.0f, 0.0f),
			FVector(0.0f, 1400.0f, 0.0f),
			FVector(-1400.0f, 0.0f, 0.0f),
			FVector(0.0f, -1400.0f, 0.0f),
			FVector(2000.0f, 2000.0f, 0.0f)
		};
		GPBuildingDropAuthority::FDeployResult Deployed;
		for (const FVector& Offset : DeployOffsets)
		{
			ValidDeployLocation = RejectDeployLocation + Offset;
			Deployed = GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				TurretDrop,
				FTransform(FRotator::ZeroRotator, ValidDeployLocation));
			if (Deployed.bAccepted)
			{
				break;
			}
		}
		if (!Deployed.bAccepted)
		{
			UE_LOG(LogGPDefensiveTurretContract, Error,
				TEXT("Orbital deploy rejected: Reason=%d ReadyAfter=%d"),
				static_cast<int32>(Deployed.RejectReason),
				Deployed.ReadyAfter);
		}
		LastPodWeak = Deployed.SpawnedPod;
		const int32 ReadyAfterDeploy = OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(TurretDrop);
		const float OrbitalAfterDeploy = GPDefensiveTurretContractDebug::GetOrbital(OwnerPS);
		Expect(Deployed.bAccepted
			&& ReadyAfterDeploy == 0
			&& FMath::IsNearlyEqual(OrbitalAfterDeploy, OrbitalBeforeDeploy, 0.5f)
			&& Deployed.PayloadClass != nullptr
			&& Deployed.PayloadClass->IsChildOf(AGP_DefensiveTurret::StaticClass())
			&& Deployed.SpawnedPod.IsValid(),
			TEXT("Orbital_DeployConsumesReadyNoSecondSpend"));
		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 17:
	{
		AGP_DefensiveTurret* Spawned = nullptr;
		int32 Count = 0;
		for (TActorIterator<AGP_DefensiveTurret> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPDefensiveTurretContractDebug::ContractTeam && !It->IsDead())
			{
				++Count;
				Spawned = *It;
			}
		}
		OrbitalTurretWeak = Spawned;
		UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
		TArray<FIntPoint> Cells;
		const bool bRegistered = IsValid(Spawned) && Grid != nullptr
			&& Grid->GetOccupantCells(Spawned->GetGridOccupantId(), Cells) && Cells.Num() > 0;
		UGP_UnitCommandComponent* Cmd = Spawned != nullptr ? Spawned->GetUnitCommandComponent() : nullptr;
		Expect(Count == 1 && IsValid(Spawned)
			&& Spawned->GetTeamId() == GPDefensiveTurretContractDebug::ContractTeam
			&& bRegistered
			&& Cmd != nullptr
			&& Cmd->IsEligibleForCombatAutoAcquire(),
			TEXT("Orbital_SpawnedOperationalTurret"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#endif
