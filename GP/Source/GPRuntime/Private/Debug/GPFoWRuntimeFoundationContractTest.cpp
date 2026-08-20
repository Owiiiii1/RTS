// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogOfWar/GPFoWRuntimeFoundationContractTest.h"

#if !UE_BUILD_SHIPPING

#include "Combat/GPDamageApplication.h"
#include "Debug/GPContractTestCoordinator.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Player/GPPlayerState.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPWorker.h"
#include "Buildings/GPMainBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWRuntimeContract, Log, All);

namespace GPFoWRuntimeContractPrivate
{
	static constexpr int32 TeamA = 81;
	static constexpr int32 TeamB = 82;
	static constexpr int32 PlacementTeam = 83;
	static TWeakObjectPtr<UGP_FoWRuntimeFoundationContractTestRunner> GActiveRunner;

	static UGP_UnitDefinition* MakeDefinition(
		UObject* Outer,
		float FoWRadiusCm,
		bool bGrantsVision,
		float Health = 100.0f,
		float Damage = 1.0f,
		float AttackRangeCm = 600.0f,
		float CombatSightCm = 900.0f)
	{
		UGP_UnitDefinition* Definition = NewObject<UGP_UnitDefinition>(Outer);
		Definition->MaxHealth = Health;
		Definition->InitialHealth = Health;
		Definition->Damage = Damage;
		Definition->AttackRangeCm = AttackRangeCm;
		Definition->SightRangeCm = CombatSightCm;
		Definition->AttackCooldownSeconds = 5.0f;
		Definition->AutoAcquireScanIntervalSeconds = 0.1f;
		Definition->FogOfWarSightRadiusCm = FoWRadiusCm;
		Definition->bGrantsFogOfWarVision = bGrantsVision;
		return Definition;
	}

	template <typename TActor>
	static TActor* SpawnWithDefinition(
		UWorld* World,
		const FVector& Location,
		int32 TeamId,
		UGP_UnitDefinition* Definition)
	{
		if (World == nullptr || Definition == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		TActor* Actor = World->SpawnActorDeferred<TActor>(
			TActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Actor == nullptr)
		{
			return nullptr;
		}

		Actor->UnitDefinitionAsset = Definition;
		UGameplayStatics::FinishSpawningActor(Actor, FTransform(FRotator::ZeroRotator, Location));
		Actor->SetTeamId(TeamId);
		return Actor;
	}

	static AGP_PlayerState* SpawnTeamPlayerState(UWorld* World, AGP_GameState* GameState, int32 TeamId)
	{
		if (World == nullptr || GameState == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerState* PlayerState = World->SpawnActor<AGP_PlayerState>(
			AGP_PlayerState::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params);
		if (PlayerState != nullptr)
		{
			PlayerState->SetTeamId(TeamId);
			GameState->AddPlayerState(PlayerState);
		}
		return PlayerState;
	}

	static void RunRuntimeFoundationContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPFoWRuntimeContract, Warning,
				TEXT("gp.FoW.RunRuntimeFoundationContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPFoWRuntimeContract, Warning,
				TEXT("gp.FoW.RunRuntimeFoundationContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World,
			TEXT("FoWRuntimeFoundationContract"),
			TEXT("FoWRuntimeFoundation"),
			Token))
		{
			return;
		}

		UGP_FoWRuntimeFoundationContractTestRunner* Runner =
			NewObject<UGP_FoWRuntimeFoundationContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GRunRuntimeFoundationContract(
		TEXT("gp.FoW.RunRuntimeFoundationContractTest"),
		TEXT("Run authoritative three-state per-team Fog of War runtime foundation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRuntimeFoundationContractTest));
}

void UGP_FoWRuntimeFoundationContractTestRunner::BeginDestroy()
{
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_FoWRuntimeFoundationContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_FoWRuntimeFoundationContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
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

void UGP_FoWRuntimeFoundationContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		if (AGP_GameState* GameState = World->GetGameState<AGP_GameState>())
		{
			if (AGP_PlayerState* PlayerState = PlacementPlayerStateWeak.Get())
			{
				GameState->RemovePlayerState(PlayerState);
			}
		}
	}

	TWeakObjectPtr<AActor> Actors[] = {
		PrimarySourceWeak,
		UnionSourceWeak,
		NonVisionSourceWeak,
		DeadSourceWeak,
		DamageSourceWeak,
		AutoAcquireOwnerWeak,
		AutoAcquireTargetWeak,
		AutoAcquireRevealSourceWeak,
		PlacementMainBaseWeak,
		PlacementPlayerStateWeak
	};
	for (TWeakObjectPtr<AActor>& ActorWeak : Actors)
	{
		if (AActor* Actor = ActorWeak.Get())
		{
			Actor->Destroy();
		}
	}

	PrimarySourceWeak.Reset();
	UnionSourceWeak.Reset();
	NonVisionSourceWeak.Reset();
	DeadSourceWeak.Reset();
	DamageSourceWeak.Reset();
	AutoAcquireOwnerWeak.Reset();
	AutoAcquireTargetWeak.Reset();
	AutoAcquireRevealSourceWeak.Reset();
	PlacementMainBaseWeak.Reset();
	PlacementPlayerStateWeak.Reset();
	TestDefinitions.Reset();
}

void UGP_FoWRuntimeFoundationContractTestRunner::Finish()
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
	UE_LOG(LogGPFoWRuntimeContract, Log,
		TEXT("gp.FoW.RunRuntimeFoundationContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPFoWRuntimeContractPrivate::GActiveRunner.Reset();
}

void UGP_FoWRuntimeFoundationContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPFoWRuntimeContract, Error,
		TEXT("gp.FoW.RunRuntimeFoundationContractTest ABORT: %s"),
		Reason);
	++Failures;
	Finish();
}

bool UGP_FoWRuntimeFoundationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPFoWRuntimeContract, Error,
			TEXT("gp.FoW.RunRuntimeFoundationContractTest FAIL: %s"),
			Label);
		return false;
	}
	UE_LOG(LogGPFoWRuntimeContract, Log,
		TEXT("gp.FoW.RunRuntimeFoundationContractTest PASS: %s"),
		Label);
	return true;
}

void UGP_FoWRuntimeFoundationContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_FoWRuntimeFoundationContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_FoWRuntimeFoundationContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UGP_FoWRuntimeFoundationContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPFoWRuntimeContract, Log, TEXT("gp.FoW.RunRuntimeFoundationContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_FoWRuntimeFoundationContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	AGP_GameState* GameState = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	UGP_FogOfWarComponent* FoW = GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
	if (World == nullptr || GameState == nullptr || FoW == nullptr)
	{
		Abort(TEXT("MissingWorldGameStateOrFoW"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		Expect(World->GetNetMode() != NM_Client, TEXT("Authority_ServerWorld"));
		Expect(FMath::IsNearlyEqual(FoW->GetUpdateIntervalSeconds(), 0.2f, KINDA_SMALL_NUMBER),
			TEXT("Update_RateFiveHz"));
		Expect(FoW->GetCellSizeCm() == 200.0f, TEXT("Grid_CellSize200"));

		const FVector Seed(-70000.0f, -20000.0f, 100.0f);
		FIntPoint SeedCell;
		if (!Expect(FoW->WorldToCell(Seed, SeedCell), TEXT("Grid_SeedInBounds")))
		{
			Finish();
			return;
		}
		PrimaryOriginalLocation = FoW->GetCellCenterWorld(SeedCell, 100.0f);
		PrimaryMovedLocation = PrimaryOriginalLocation + FVector(1200.0f, 0.0f, 0.0f);
		AutoAcquireLocation = PrimaryOriginalLocation + FVector(6000.0f, 0.0f, 0.0f);

		Expect(
			FoW->GetStateForTeamAtWorldLocation(
				GPFoWRuntimeContractPrivate::TeamA,
				PrimaryOriginalLocation + FVector(10000.0f, 0.0f, 0.0f))
				== EGP_FoWState::Unexplored,
			TEXT("State_DefaultUnexplored"));

		UGP_UnitDefinition* VisionDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 450.0f, true);
		UGP_UnitDefinition* NonVisionDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 900.0f, false);
		UGP_UnitDefinition* FragileVisionDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 450.0f, true, 10.0f);
		UGP_UnitDefinition* DamageDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 0.0f, false, 100.0f, 100.0f);
		UGP_UnitDefinition* AutoAcquireDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 0.0f, false, 200.0f, 1.0f, 600.0f, 900.0f);
		UGP_UnitDefinition* DurableTargetDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 450.0f, true, 1000.0f, 1.0f);
		TestDefinitions = {
			VisionDef,
			NonVisionDef,
			FragileVisionDef,
			DamageDef,
			AutoAcquireDef,
			DurableTargetDef
		};

		PrimarySourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World, PrimaryOriginalLocation, GPFoWRuntimeContractPrivate::TeamA, VisionDef);
		UnionSourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World,
			PrimaryOriginalLocation + FVector(0.0f, 1400.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamA,
			VisionDef);
		NonVisionSourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World,
			PrimaryOriginalLocation + FVector(3000.0f, 0.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamA,
			NonVisionDef);
		DeadSourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World,
			PrimaryOriginalLocation + FVector(4200.0f, 0.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamA,
			FragileVisionDef);
		DamageSourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_SalvageWalker>(
			World,
			PrimaryOriginalLocation + FVector(4600.0f, 0.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamB,
			DamageDef);
		AutoAcquireOwnerWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_SalvageWalker>(
			World,
			AutoAcquireLocation,
			GPFoWRuntimeContractPrivate::TeamA,
			AutoAcquireDef);
		AutoAcquireTargetWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World,
			AutoAcquireLocation + FVector(300.0f, 0.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamB,
			DurableTargetDef);
		AutoAcquireRevealSourceWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_Worker>(
			World,
			AutoAcquireLocation + FVector(2200.0f, 0.0f, 0.0f),
			GPFoWRuntimeContractPrivate::TeamA,
			VisionDef);

		if (!Expect(
			PrimarySourceWeak.IsValid()
				&& UnionSourceWeak.IsValid()
				&& NonVisionSourceWeak.IsValid()
				&& DeadSourceWeak.IsValid()
				&& DamageSourceWeak.IsValid()
				&& AutoAcquireOwnerWeak.IsValid()
				&& AutoAcquireTargetWeak.IsValid()
				&& AutoAcquireRevealSourceWeak.IsValid(),
			TEXT("Sources_Spawned")))
		{
			Finish();
			return;
		}

		FoW->RecomputeVisibilityNow();
		Expect(FoW->IsVisibleToTeam(GPFoWRuntimeContractPrivate::TeamA, PrimaryOriginalLocation),
			TEXT("State_LiveSourceVisible"));
		Expect(FoW->IsExploredByTeam(GPFoWRuntimeContractPrivate::TeamA, PrimaryOriginalLocation),
			TEXT("State_VisibleImpliesExplored"));
		Expect(!FoW->IsExploredByTeam(GPFoWRuntimeContractPrivate::TeamB, PrimaryOriginalLocation),
			TEXT("Teams_IndependentNoLeakage"));
		Expect(
			FoW->IsVisibleToTeam(
				GPFoWRuntimeContractPrivate::TeamA,
				PrimaryOriginalLocation + FVector(400.0f, 0.0f, 0.0f)),
			TEXT("Radius_DeterministicInsideCell"));
		Expect(
			!FoW->IsVisibleToTeam(
				GPFoWRuntimeContractPrivate::TeamA,
				PrimaryOriginalLocation + FVector(600.0f, 0.0f, 0.0f)),
			TEXT("Radius_DeterministicOutsideCell"));
		Expect(
			FoW->IsVisibleToTeam(
				GPFoWRuntimeContractPrivate::TeamA,
				UnionSourceWeak->GetActorLocation()),
			TEXT("Sources_MultipleUnion"));
		Expect(
			!FoW->IsVisibleToTeam(
				GPFoWRuntimeContractPrivate::TeamA,
				NonVisionSourceWeak->GetActorLocation()),
			TEXT("Sources_NonVisionIgnored"));

		PrimarySourceWeak->SetActorLocation(PrimaryMovedLocation);
		FoW->RecomputeVisibilityNow();
		Expect(
			FoW->GetStateForTeamAtWorldLocation(
				GPFoWRuntimeContractPrivate::TeamA,
				PrimaryOriginalLocation) == EGP_FoWState::Explored,
			TEXT("State_VisibleDowngradesToExplored"));
		Expect(FoW->IsVisibleToTeam(GPFoWRuntimeContractPrivate::TeamA, PrimaryMovedLocation),
			TEXT("State_MovedSourceNewAreaVisible"));

		FGP_DamageApplicationResult DamageResult;
		DeadSourceWeak->ApplyDamageFromUnit(DamageSourceWeak.Get(), DamageResult);
		FoW->RecomputeVisibilityNow();
		Expect(DeadSourceWeak->IsDead(), TEXT("Sources_DeathApplied"));
		Expect(
			FoW->GetStateForTeamAtWorldLocation(
				GPFoWRuntimeContractPrivate::TeamA,
				DeadSourceWeak->GetActorLocation()) == EGP_FoWState::Explored,
			TEXT("Sources_DeadContributesNothing"));
		Expect(
			FoW->GetStateForTeamAtWorldLocation(0, PrimaryMovedLocation) == EGP_FoWState::Unexplored,
			TEXT("Query_InvalidTeamSafe"));
		Expect(
			FoW->GetStateForTeamAtWorldLocation(
				GPFoWRuntimeContractPrivate::TeamA,
				FVector(NAN, 0.0f, 0.0f)) == EGP_FoWState::Unexplored,
			TEXT("Query_InvalidLocationSafe"));

		if (UGP_UnitCommandComponent* Command = AutoAcquireOwnerWeak->GetUnitCommandComponent())
		{
			Command->AutoAcquireScanIntervalSeconds = 0.1f;
			Command->AutoAcquireSightRangeCm = 900.0f;
			Command->RefreshCombatAutoAcquireTimer();
		}

		++StageIndex;
		ScheduleNext(0.45f);
		break;
	}
	case 1:
	{
		AGP_SalvageWalker* Owner = AutoAcquireOwnerWeak.Get();
		AGP_Worker* Target = AutoAcquireTargetWeak.Get();
		AGP_Worker* RevealSource = AutoAcquireRevealSourceWeak.Get();
		if (!Expect(IsValid(Owner) && IsValid(Target) && IsValid(RevealSource),
			TEXT("AutoAcquire_HiddenSetupAlive")))
		{
			Finish();
			return;
		}

		UGP_UnitCommandComponent* Command = Owner->GetUnitCommandComponent();
		Expect(!FoW->IsVisibleToTeam(
			GPFoWRuntimeContractPrivate::TeamA,
			Target->GetActorLocation()), TEXT("AutoAcquire_TargetInitiallyHidden"));
		Expect(Command != nullptr && !Command->IsAttackActive(), TEXT("AutoAcquire_HiddenRejected"));
		Expect(Command != nullptr && Command->DebugGetLastAutoAcquireCandidate() == nullptr,
			TEXT("AutoAcquire_NoHiddenCandidate"));

		RevealSource->SetActorLocation(AutoAcquireLocation);
		FoW->RecomputeVisibilityNow();
		Expect(FoW->IsVisibleToTeam(
			GPFoWRuntimeContractPrivate::TeamA,
			Target->GetActorLocation()), TEXT("AutoAcquire_TargetRevealed"));

		++StageIndex;
		ScheduleNext(0.45f);
		break;
	}
	case 2:
	{
		AGP_SalvageWalker* Owner = AutoAcquireOwnerWeak.Get();
		AGP_Worker* Target = AutoAcquireTargetWeak.Get();
		if (!Expect(IsValid(Owner) && IsValid(Target), TEXT("AutoAcquire_VisibleSetupAlive")))
		{
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Command = Owner->GetUnitCommandComponent();
		Expect(Command != nullptr && Command->IsAttackActive(), TEXT("AutoAcquire_VisibleAccepted"));
		Expect(Command != nullptr && Command->GetAttackTarget() == Target,
			TEXT("AutoAcquire_VisibleTargetSelected"));

		UGP_UnitDefinition* MainBaseDef =
			GPFoWRuntimeContractPrivate::MakeDefinition(this, 3000.0f, true, 1000.0f, 0.0f, 0.0f, 0.0f);
		TestDefinitions.Add(MainBaseDef);
		const FVector BaseLocation(-46000.0f, 8000.0f, 100.0f);
		PlacementMainBaseWeak = GPFoWRuntimeContractPrivate::SpawnWithDefinition<AGP_MainBase>(
			World,
			BaseLocation,
			GPFoWRuntimeContractPrivate::PlacementTeam,
			MainBaseDef);
		PlacementPlayerStateWeak = GPFoWRuntimeContractPrivate::SpawnTeamPlayerState(
			World,
			GameState,
			GPFoWRuntimeContractPrivate::PlacementTeam);
		if (!Expect(PlacementMainBaseWeak.IsValid() && PlacementPlayerStateWeak.IsValid(),
			TEXT("Placement_SetupSpawned")))
		{
			Finish();
			return;
		}

		PlacementVisibleLocation = BaseLocation + FVector(1400.0f, 0.0f, 0.0f);
		PlacementHiddenLocation = BaseLocation + FVector(4500.0f, 0.0f, 0.0f);
		FoW->RecomputeVisibilityNow();

		UGP_OrbitalDropDefinition* HubDrop =
			UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		if (!Expect(IsValid(HubDrop), TEXT("Placement_HubDefinitionPresent")))
		{
			Finish();
			return;
		}

		EGP_BuildingDropRejectReason Reject = EGP_BuildingDropRejectReason::None;
		const bool bHiddenAccepted = GPBuildingDropAuthority::ValidateBuildingPlacement(
			World,
			PlacementPlayerStateWeak.Get(),
			HubDrop,
			FTransform(FRotator::ZeroRotator, PlacementHiddenLocation),
			Reject);
		Expect(!bHiddenAccepted && Reject == EGP_BuildingDropRejectReason::NotVisible,
			TEXT("Placement_HiddenAuthorityRejected"));

		Reject = EGP_BuildingDropRejectReason::None;
		const bool bVisibleAccepted = GPBuildingDropAuthority::ValidateBuildingPlacement(
			World,
			PlacementPlayerStateWeak.Get(),
			HubDrop,
			FTransform(FRotator::ZeroRotator, PlacementVisibleLocation),
			Reject);
		Expect(bVisibleAccepted && Reject == EGP_BuildingDropRejectReason::None,
			TEXT("Placement_VisibleOtherwiseValidAccepted"));

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_FoWRuntimeFoundationContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_FoWRuntimeFoundationContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_FoWRuntimeFoundationContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_FoWRuntimeFoundationContractTestRunner::AdvanceStage() {}
bool UGP_FoWRuntimeFoundationContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_FoWRuntimeFoundationContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_FoWRuntimeFoundationContractTestRunner::Finish() { bFinished = true; }
void UGP_FoWRuntimeFoundationContractTestRunner::OnWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_FoWRuntimeFoundationContractTestRunner::UnbindWorldCleanup() {}
void UGP_FoWRuntimeFoundationContractTestRunner::CleanupActors() {}

#endif
