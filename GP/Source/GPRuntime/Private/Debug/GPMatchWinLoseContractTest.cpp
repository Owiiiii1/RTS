// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPMatchWinLoseContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Combat/GPDamageApplication.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddScore.h"
#include "Effects/GPGE_DamageBasic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameMode.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPStorageComponent.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMatchWinLose, Log, All);

namespace GPMatchWinLoseDebug
{
	static TWeakObjectPtr<UGP_MatchWinLoseContractTestRunner> GActiveRunner;
	constexpr int32 TeamA = 81;
	constexpr int32 TeamB = 82;
	constexpr int32 KnownSeed = 424242;

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

	static AGP_MainBase* SpawnTeamMainBase(UWorld* World, int32 TeamId, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (IsValid(Base))
		{
			Base->SetTeamId(TeamId);
		}
		return Base;
	}

	static void SetAttr(AGP_PlayerState* PS, const FGameplayAttribute& Attr, float Value)
	{
		if (!IsValid(PS))
		{
			return;
		}
		if (UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(Attr, Value);
		}
	}

	static void SetScore(AGP_PlayerState* PS, float Value)
	{
		SetAttr(PS, UGP_PlayerAttributeSet::GetFerroniteScoreAttribute(), Value);
	}

	static void SetOrbital(AGP_PlayerState* PS, float Value)
	{
		SetAttr(PS, UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute(), Value);
	}

	static void SetUnits(AGP_PlayerState* PS, float Value)
	{
		SetAttr(PS, UGP_PlayerAttributeSet::GetCurrentUnitsAttribute(), Value);
	}

	static void AddScoreGE(AGP_PlayerState* PS, float Amount)
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
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGP_GE_AddScore::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(UGP_GE_AddScore::GetMagnitudeDataName(), Amount);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	static int32 ScoreOf(const AGP_PlayerState* PS)
	{
		if (PS == nullptr || PS->GetPlayerAttributeSet() == nullptr)
		{
			return -1;
		}
		return FMath::RoundToInt(PS->GetPlayerAttributeSet()->GetFerroniteScore());
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

	static AGP_PlayerState* FindPlayableByTeam(UWorld* World, int32 TeamId)
	{
		return AGP_PlayerState::FindAuthoritativeForTeam(World, TeamId);
	}

	static bool IsPlaying(const AGP_GameState* GS)
	{
		return GS != nullptr && GS->GetMatchStateTag() == FGPGameplayTags::Get().Match_State_Playing;
	}

	static bool IsFinished(const AGP_GameState* GS)
	{
		return GS != nullptr && GS->IsMatchFinished();
	}

	static const FGP_MatchTeamScore* FindFinal(const FGP_MatchResult& Result, int32 TeamId)
	{
		for (const FGP_MatchTeamScore& Entry : Result.FinalScores)
		{
			if (Entry.TeamId == TeamId)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	static void RunWinLoseContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("gp.Match.RunWinLoseContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("gp.Match.RunWinLoseContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("MatchWinLoseContract"), TEXT("MatchWinLose"), Token))
		{
			return;
		}

		UGP_MatchWinLoseContractTestRunner* Runner =
			NewObject<UGP_MatchWinLoseContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static AGP_GameMode* GetAuthGM(UWorld* World)
	{
		return World != nullptr ? World->GetAuthGameMode<AGP_GameMode>() : nullptr;
	}

	static void DebugSetFerroniteScore(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetFerroniteScore denied (client or missing world)."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("Usage: gp.Match.DebugSetFerroniteScore <Amount> [TeamId]"));
			return;
		}
		float Amount = 0.0f;
		if (!FDefaultValueHelper::ParseFloat(Args[0], Amount))
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetFerroniteScore: invalid amount."));
			return;
		}
		int32 TeamId = INDEX_NONE;
		if (Args.Num() >= 2)
		{
			FDefaultValueHelper::ParseInt(Args[1], TeamId);
		}
		AGP_PlayerState* PS = nullptr;
		if (TeamId >= 1)
		{
			PS = FindPlayableByTeam(World, TeamId);
		}
		else if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PS = PC->GetPlayerState<AGP_PlayerState>();
		}
		if (!IsValid(PS))
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetFerroniteScore: no PlayerState."));
			return;
		}
		SetScore(PS, Amount);
		UE_LOG(LogGPMatchWinLose, Warning,
			TEXT("GP Match Debug: DebugSetFerroniteScore TeamId=%d Amount=%.0f (authority-only, non-shipping)."),
			PS->GetTeamId(), Amount);
	}

	static void DebugSetMatchTimeRemaining(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetMatchTimeRemaining denied (client or missing world)."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("Usage: gp.Match.DebugSetMatchTimeRemaining <Seconds>"));
			return;
		}
		float Seconds = 0.0f;
		if (!FDefaultValueHelper::ParseFloat(Args[0], Seconds))
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetMatchTimeRemaining: invalid seconds."));
			return;
		}
		AGP_GameMode* GM = GetAuthGM(World);
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (GM == nullptr || GS == nullptr || !GM->HasAuthority())
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetMatchTimeRemaining: missing authority match actors."));
			return;
		}
		GS->SetMatchTimeRemaining(Seconds);
		UE_LOG(LogGPMatchWinLose, Warning,
			TEXT("GP Match Debug: DebugSetMatchTimeRemaining Seconds=%.1f (authority-only, non-shipping)."),
			Seconds);
		if (Seconds <= 0.0f && IsPlaying(GS))
		{
			GM->HandleMatchTimeExpired();
		}
	}

	static void DebugKillMainBase(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugKillMainBase denied (client or missing world)."));
			return;
		}
		int32 TeamId = INDEX_NONE;
		if (Args.Num() >= 1)
		{
			FDefaultValueHelper::ParseInt(Args[0], TeamId);
		}
		if (TeamId < 1)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (const AGP_PlayerState* PS = PC->GetPlayerState<AGP_PlayerState>())
				{
					TeamId = PS->GetTeamId();
				}
			}
		}
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* Base = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		if (!IsValid(Base))
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugKillMainBase: no MainBase for TeamId=%d."), TeamId);
			return;
		}
		const bool bDead = KillUnit(Base);
		UE_LOG(LogGPMatchWinLose, Warning,
			TEXT("GP Match Debug: DebugKillMainBase TeamId=%d Dead=%s (authority-only, non-shipping)."),
			TeamId, bDead ? TEXT("true") : TEXT("false"));
	}

	static void DebugSetMatchSeed(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetMatchSeed denied (client or missing world)."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("Usage: gp.Match.DebugSetMatchSeed <Seed>"));
			return;
		}
		int32 Seed = 0;
		if (!FDefaultValueHelper::ParseInt(Args[0], Seed))
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugSetMatchSeed: invalid seed."));
			return;
		}
		AGP_GameMode* GM = GetAuthGM(World);
		if (GM == nullptr || !GM->HasAuthority())
		{
			return;
		}
		GM->DebugSetMatchSeed(Seed);
		UE_LOG(LogGPMatchWinLose, Warning,
			TEXT("GP Match Debug: DebugSetMatchSeed Seed=%d (authority-only, non-shipping)."), Seed);
	}

	static void DebugStart(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugStart denied (client or missing world)."));
			return;
		}

		AGP_GameMode* GM = GetAuthGM(World);
		if (GM == nullptr || !GM->HasAuthority())
		{
			UE_LOG(LogGPMatchWinLose, Warning, TEXT("GP Match DebugStart: missing authority GameMode."));
			return;
		}

		GM->DebugStartMatchFlow();
	}

	static FAutoConsoleCommandWithWorldAndArgs GWinLoseContract(
		TEXT("gp.Match.RunWinLoseContractTest"),
		TEXT("Authority: GP-S34W match win/lose contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunWinLoseContractTest));

	static FAutoConsoleCommandWithWorldAndArgs GDebugSetScore(
		TEXT("gp.Match.DebugSetFerroniteScore"),
		TEXT("DEVELOPMENT ONLY. Authority: set FerroniteScore for local or TeamId."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSetFerroniteScore));

	static FAutoConsoleCommandWithWorldAndArgs GDebugSetTime(
		TEXT("gp.Match.DebugSetMatchTimeRemaining"),
		TEXT("DEVELOPMENT ONLY. Authority: set MatchTimeRemaining; 0 triggers timer evaluation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSetMatchTimeRemaining));

	static FAutoConsoleCommandWithWorldAndArgs GDebugKillBase(
		TEXT("gp.Match.DebugKillMainBase"),
		TEXT("DEVELOPMENT ONLY. Authority: kill MainBase for local or TeamId through damage GE."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugKillMainBase));

	static FAutoConsoleCommandWithWorldAndArgs GDebugSetSeed(
		TEXT("gp.Match.DebugSetMatchSeed"),
		TEXT("DEVELOPMENT ONLY. Authority: overwrite MatchSeed used by final tie-break."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSetMatchSeed));

	static FAutoConsoleCommandWithWorldAndArgs GDebugStart(
		TEXT("gp.Match.DebugStart"),
		TEXT("DEVELOPMENT ONLY. Authority: StartMatchFlow from WaitingForPlayers without changing ExpectedHumanPlayers."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugStart));
}

void UGP_MatchWinLoseContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_MatchWinLoseContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_MatchWinLoseContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_MatchWinLoseContractTestRunner::RestoreLiveTeams()
{
	if (!bIsolatedLiveTeams)
	{
		return;
	}
	for (int32 Index = 0; Index < SavedLivePlayerStates.Num(); ++Index)
	{
		if (AGP_PlayerState* PS = SavedLivePlayerStates[Index].Get())
		{
			PS->SetTeamId(SavedLiveTeamIds.IsValidIndex(Index) ? SavedLiveTeamIds[Index] : -1);
		}
	}
	SavedLivePlayerStates.Reset();
	SavedLiveTeamIds.Reset();
	bIsolatedLiveTeams = false;
}

void UGP_MatchWinLoseContractTestRunner::CleanupActors()
{
	UWorld* World = WorldWeak.Get();
	if (World != nullptr)
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (AGP_PlayerState* A = TeamAStateWeak.Get())
			{
				GS->RemovePlayerState(A);
			}
			if (AGP_PlayerState* B = TeamBStateWeak.Get())
			{
				GS->RemovePlayerState(B);
			}
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
	DestroyWeak(ExtraUnitWeak);
	DestroyWeak(ExtraHubWeak);
	DestroyWeak(MainBaseAWeak);
	DestroyWeak(MainBaseBWeak);
	DestroyWeak(TeamAStateWeak);
	DestroyWeak(TeamBStateWeak);
}

void UGP_MatchWinLoseContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (AGP_GameMode* GM = GPMatchWinLoseDebug::GetAuthGM(WorldWeak.Get()))
	{
		GM->DebugSetDeliveryQuotaFerroniteScore(5000.0f);
		GM->DebugSetAnnihilationCountsAsWin(true);
		GM->DebugResetMatchFlowToWaiting();
	}
	RestoreLiveTeams();
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
	UE_LOG(LogGPMatchWinLose, Log,
		TEXT("gp.Match.RunWinLoseContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));
	RemoveFromRoot();
	GPMatchWinLoseDebug::GActiveRunner.Reset();
}

void UGP_MatchWinLoseContractTestRunner::Abort(const TCHAR* Reason)
{
	++Failures;
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPMatchWinLose, Error, TEXT("gp.Match.RunWinLoseContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_MatchWinLoseContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPMatchWinLose, Error, TEXT("gp.Match.RunWinLoseContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPMatchWinLose, Log, TEXT("gp.Match.RunWinLoseContractTest PASS: %s"), Label);
	return true;
}

void UGP_MatchWinLoseContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_MatchWinLoseContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_MatchWinLoseContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_MatchWinLoseContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMatchWinLose, Log, TEXT("gp.Match.RunWinLoseContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

bool UGP_MatchWinLoseContractTestRunner::IsolateLivePlayableTeams()
{
	UWorld* World = WorldWeak.Get();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		return false;
	}
	SavedLivePlayerStates.Reset();
	SavedLiveTeamIds.Reset();
	for (APlayerState* Candidate : GS->PlayerArray)
	{
		AGP_PlayerState* GPPS = Cast<AGP_PlayerState>(Candidate);
		if (!IsValid(GPPS) || GPPS->GetTeamId() < 1)
		{
			continue;
		}
		SavedLivePlayerStates.Add(GPPS);
		SavedLiveTeamIds.Add(GPPS->GetTeamId());
	}
	for (int32 Index = 0; Index < SavedLivePlayerStates.Num(); ++Index)
	{
		if (AGP_PlayerState* PS = SavedLivePlayerStates[Index].Get())
		{
			PS->SetTeamId(-1);
		}
	}
	bIsolatedLiveTeams = true;
	return true;
}

bool UGP_MatchWinLoseContractTestRunner::ResetAndStartMatch()
{
	UWorld* World = WorldWeak.Get();
	AGP_GameMode* GM = GPMatchWinLoseDebug::GetAuthGM(World);
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return false;
	}
	GM->DebugResetMatchFlowToWaiting();
	GM->DebugSetDeliveryQuotaFerroniteScore(5000.0f);
	GM->DebugSetAnnihilationCountsAsWin(true);
	if (AGP_PlayerState* A = TeamAStateWeak.Get())
	{
		GPMatchWinLoseDebug::SetScore(A, 0.0f);
		GPMatchWinLoseDebug::SetOrbital(A, 0.0f);
		GPMatchWinLoseDebug::SetUnits(A, 0.0f);
	}
	if (AGP_PlayerState* B = TeamBStateWeak.Get())
	{
		GPMatchWinLoseDebug::SetScore(B, 0.0f);
		GPMatchWinLoseDebug::SetOrbital(B, 0.0f);
		GPMatchWinLoseDebug::SetUnits(B, 0.0f);
	}
	GM->StartMatchFlow();
	GM->StopMatchCountdown();
	GM->DebugSetMatchSeed(GPMatchWinLoseDebug::KnownSeed);
	return GPMatchWinLoseDebug::IsPlaying(GS) && !GS->GetMatchResult().HasWinner();
}

void UGP_MatchWinLoseContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	AGP_GameMode* GM = GPMatchWinLoseDebug::GetAuthGM(World);
	if (!Expect(IsValid(GS) && IsValid(GM), TEXT("MatchActorsPresent")))
	{
		Finish();
		return;
	}

	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	const FVector PadA(-56000.0f, -18000.0f, 100.0f);
	const FVector PadB(-56000.0f, -16000.0f, 100.0f);

	switch (StageIndex)
	{
	case 0: // A canonical defaults + DebugStart seam
	{
		Expect(FMath::IsNearlyEqual(GM->GetMatchDurationSeconds(), 600.0f), TEXT("A_Duration600"));
		Expect(FMath::IsNearlyEqual(GM->GetDeliveryQuotaFerroniteScore(), 5000.0f), TEXT("A_Quota5000"));
		Expect(GM->GetAnnihilationCountsAsWin(), TEXT("A_AnnihilationTrue"));
		Expect(GM->GetExpectedHumanPlayers() == 2, TEXT("A_ExpectedHumanPlayers2"));
		Expect(FMath::IsNearlyEqual(GS->GetDeliveryQuotaFerroniteScore(), 5000.0f), TEXT("A_GSQuota5000"));
		Expect(GS->GetAnnihilationCountsAsWin(), TEXT("A_GSAnnihilationTrue"));

		GM->DebugResetMatchFlowToWaiting();
		Expect(GS->GetMatchStateTag() == Tags.Match_State_WaitingForPlayers, TEXT("P_WaitingWithoutDebugStart"));
		Expect(FMath::IsNearlyEqual(GS->GetMatchTimeRemaining(), 0.0f), TEXT("P_WaitingTimerZero"));
		Expect(GM->GetExpectedHumanPlayers() == 2, TEXT("P_ExpectedHumanPlayersUnchangedWhileWaiting"));

		GM->DebugStartMatchFlow();
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("P_DebugStartWaitingToPlaying"));
		Expect(FMath::IsNearlyEqual(GS->GetMatchTimeRemaining(), 600.0f), TEXT("P_DebugStartTimer600"));
		Expect(GS->GetWinnerTeamId() == -1 && !GS->GetWinReasonTag().IsValid(), TEXT("P_DebugStartResultClear"));
		Expect(GM->GetExpectedHumanPlayers() == 2, TEXT("P_ExpectedHumanPlayersStill2AfterStart"));

		GS->SetMatchTimeRemaining(500.0f);
		GM->DebugStartMatchFlow();
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("P_RepeatDebugStartStillPlaying"));
		Expect(FMath::IsNearlyEqual(GS->GetMatchTimeRemaining(), 500.0f), TEXT("P_RepeatDebugStartDoesNotResetTimer"));
		Expect(GS->GetWinnerTeamId() == -1, TEXT("P_RepeatDebugStartDoesNotResetResult"));

		GM->FinishMatch(1, Tags.Match_WinReason_TimerScore);
		Expect(GPMatchWinLoseDebug::IsFinished(GS), TEXT("P_FinishedForDebugStartReject"));
		const int32 FinishedWinner = GS->GetWinnerTeamId();
		const FGameplayTag FinishedReason = GS->GetWinReasonTag();
		const float FinishedRemaining = GS->GetMatchTimeRemaining();
		GM->DebugStartMatchFlow();
		Expect(GPMatchWinLoseDebug::IsFinished(GS), TEXT("P_DebugStartCannotRestartFinished"));
		Expect(GS->GetWinnerTeamId() == FinishedWinner, TEXT("P_FinishedWinnerStable"));
		Expect(GS->GetWinReasonTag() == FinishedReason, TEXT("P_FinishedReasonStable"));
		Expect(FMath::IsNearlyEqual(GS->GetMatchTimeRemaining(), FinishedRemaining), TEXT("P_FinishedTimerStable"));
		Expect(GM->GetExpectedHumanPlayers() == 2, TEXT("P_ExpectedHumanPlayersStill2AfterFinished"));

		GM->DebugResetMatchFlowToWaiting();
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1: // B start + isolate
	{
		if (!Expect(IsolateLivePlayableTeams(), TEXT("B_IsolateLiveTeams")))
		{
			Finish();
			return;
		}
		AGP_PlayerState* A = GPMatchWinLoseDebug::SpawnTeamPlayerState(World, GS, GPMatchWinLoseDebug::TeamA);
		AGP_PlayerState* B = GPMatchWinLoseDebug::SpawnTeamPlayerState(World, GS, GPMatchWinLoseDebug::TeamB);
		TeamAStateWeak = A;
		TeamBStateWeak = B;
		MainBaseAWeak = GPMatchWinLoseDebug::SpawnTeamMainBase(World, GPMatchWinLoseDebug::TeamA, PadA);
		MainBaseBWeak = GPMatchWinLoseDebug::SpawnTeamMainBase(World, GPMatchWinLoseDebug::TeamB, PadB);
		if (!Expect(IsValid(A) && IsValid(B) && MainBaseAWeak.IsValid() && MainBaseBWeak.IsValid(), TEXT("B_SpawnHarness")))
		{
			Finish();
			return;
		}
		if (!Expect(ResetAndStartMatch(), TEXT("B_StartPlaying")))
		{
			Finish();
			return;
		}
		Expect(FMath::IsNearlyEqual(GS->GetMatchTimeRemaining(), 600.0f), TEXT("B_TimerInitialized"));
		Expect(GS->GetWinnerTeamId() == -1 && !GS->GetWinReasonTag().IsValid(), TEXT("B_ResultClear"));
		Expect(GS->GetMatchSeed() == GPMatchWinLoseDebug::KnownSeed, TEXT("B_SeedPublished"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2: // C below quota + D orbital is not victory score
	{
		AGP_PlayerState* A = TeamAStateWeak.Get();
		AGP_PlayerState* B = TeamBStateWeak.Get();
		GPMatchWinLoseDebug::SetScore(A, 4999.0f);
		GPMatchWinLoseDebug::SetOrbital(A, 99999.0f);
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("C_BelowQuotaStillPlaying"));
		Expect(GS->GetWinnerTeamId() == -1, TEXT("C_NoWinnerYet"));
		Expect(GPMatchWinLoseDebug::ScoreOf(A) == 4999, TEXT("D_ScoreUnchangedByOrbital"));
		GPMatchWinLoseDebug::SetOrbital(B, 88888.0f);
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("D_OrbitalDoesNotFinish"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3: // C cross quota via GE + exact-once + M snapshot
	{
		AGP_PlayerState* A = TeamAStateWeak.Get();
		AGP_PlayerState* B = TeamBStateWeak.Get();
		GPMatchWinLoseDebug::AddScoreGE(A, 1.0f);
		Expect(GPMatchWinLoseDebug::IsFinished(GS), TEXT("C_CrossQuotaFinished"));
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamA, TEXT("C_WinnerTeamA"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_DeliveryQuota, TEXT("C_ReasonDeliveryQuota"));
		Expect(GS->GetMatchResult().HasWinner(), TEXT("C_MatchResultHasWinner"));
		const FGP_MatchResult Snapshot = GS->GetMatchResult();
		const FGP_MatchTeamScore* FinalA = GPMatchWinLoseDebug::FindFinal(Snapshot, GPMatchWinLoseDebug::TeamA);
		Expect(FinalA != nullptr && FMath::RoundToInt(FinalA->FerroniteScore) == 5000, TEXT("M_SnapshotScore5000"));
		GPMatchWinLoseDebug::SetScore(B, 9000.0f);
		GM->FinishMatch(GPMatchWinLoseDebug::TeamB, Tags.Match_WinReason_TimerScore);
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamA, TEXT("L_SecondFinishDoesNotOverwrite"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_DeliveryQuota, TEXT("L_ReasonStable"));
		const FGP_MatchTeamScore* AfterMutate = GPMatchWinLoseDebug::FindFinal(GS->GetMatchResult(), GPMatchWinLoseDebug::TeamA);
		Expect(AfterMutate != nullptr && FMath::RoundToInt(AfterMutate->FerroniteScore) == 5000, TEXT("M_SnapshotStableAfterMutate"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4: // E timer highest FerroniteScore
	{
		if (!Expect(ResetAndStartMatch(), TEXT("E_Restart")))
		{
			Finish();
			return;
		}
		GPMatchWinLoseDebug::SetScore(TeamAStateWeak.Get(), 120.0f);
		GPMatchWinLoseDebug::SetScore(TeamBStateWeak.Get(), 340.0f);
		GM->HandleMatchTimeExpired();
		Expect(GPMatchWinLoseDebug::IsFinished(GS), TEXT("E_TimerFinished"));
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamB, TEXT("E_HighestScoreWins"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_TimerScore, TEXT("E_ReasonTimerScore"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 5: // F Ferronite tie → Orbital
	{
		if (!Expect(ResetAndStartMatch(), TEXT("F_Restart")))
		{
			Finish();
			return;
		}
		GPMatchWinLoseDebug::SetScore(TeamAStateWeak.Get(), 10.0f);
		GPMatchWinLoseDebug::SetScore(TeamBStateWeak.Get(), 10.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamAStateWeak.Get(), 50.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamBStateWeak.Get(), 20.0f);
		GM->HandleMatchTimeExpired();
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamA, TEXT("F_OrbitalTieBreak"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_TimerScore, TEXT("F_ReasonTimerScore"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 6: // G second tie → CurrentUnits
	{
		if (!Expect(ResetAndStartMatch(), TEXT("G_Restart")))
		{
			Finish();
			return;
		}
		GPMatchWinLoseDebug::SetScore(TeamAStateWeak.Get(), 10.0f);
		GPMatchWinLoseDebug::SetScore(TeamBStateWeak.Get(), 10.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamAStateWeak.Get(), 5.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamBStateWeak.Get(), 5.0f);
		GPMatchWinLoseDebug::SetUnits(TeamAStateWeak.Get(), 1.0f);
		GPMatchWinLoseDebug::SetUnits(TeamBStateWeak.Get(), 4.0f);
		GM->HandleMatchTimeExpired();
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamB, TEXT("G_UnitsTieBreak"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 7: // H full tie deterministic seed
	{
		if (!Expect(ResetAndStartMatch(), TEXT("H_Restart1")))
		{
			Finish();
			return;
		}
		GPMatchWinLoseDebug::SetScore(TeamAStateWeak.Get(), 7.0f);
		GPMatchWinLoseDebug::SetScore(TeamBStateWeak.Get(), 7.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamAStateWeak.Get(), 3.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamBStateWeak.Get(), 3.0f);
		GPMatchWinLoseDebug::SetUnits(TeamAStateWeak.Get(), 2.0f);
		GPMatchWinLoseDebug::SetUnits(TeamBStateWeak.Get(), 2.0f);
		GM->DebugSetMatchSeed(GPMatchWinLoseDebug::KnownSeed);
		GM->HandleMatchTimeExpired();
		FirstSeedWinner = GS->GetWinnerTeamId();
		Expect(FirstSeedWinner == GPMatchWinLoseDebug::TeamA || FirstSeedWinner == GPMatchWinLoseDebug::TeamB, TEXT("H_NoDraw"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_TimerScore, TEXT("H_ReasonTimerScore"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 8: // H same seed same winner
	{
		if (!Expect(ResetAndStartMatch(), TEXT("H_Restart2")))
		{
			Finish();
			return;
		}
		GPMatchWinLoseDebug::SetScore(TeamAStateWeak.Get(), 7.0f);
		GPMatchWinLoseDebug::SetScore(TeamBStateWeak.Get(), 7.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamAStateWeak.Get(), 3.0f);
		GPMatchWinLoseDebug::SetOrbital(TeamBStateWeak.Get(), 3.0f);
		GPMatchWinLoseDebug::SetUnits(TeamAStateWeak.Get(), 2.0f);
		GPMatchWinLoseDebug::SetUnits(TeamBStateWeak.Get(), 2.0f);
		GM->DebugSetMatchSeed(GPMatchWinLoseDebug::KnownSeed);
		GM->HandleMatchTimeExpired();
		Expect(GS->GetWinnerTeamId() == FirstSeedWinner, TEXT("H_SameSeedSameWinner"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 9: // J unrelated deaths do not finish
	{
		if (!Expect(ResetAndStartMatch(), TEXT("J_Restart")))
		{
			Finish();
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = TeamAStateWeak.Get();
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), PadA + FVector(400.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
		if (IsValid(Worker))
		{
			Worker->SetTeamId(GPMatchWinLoseDebug::TeamA);
		}
		ExtraUnitWeak = Worker;
		Expect(GPMatchWinLoseDebug::KillUnit(Worker), TEXT("J_KillWorker"));
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("J_WorkerDeathDoesNotFinish"));

		AGP_LogisticsHub* Hub = World->SpawnActor<AGP_LogisticsHub>(
			AGP_LogisticsHub::StaticClass(), PadA + FVector(800.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
		if (IsValid(Hub))
		{
			Hub->SetTeamId(GPMatchWinLoseDebug::TeamA);
		}
		ExtraHubWeak = Hub;
		Expect(GPMatchWinLoseDebug::KillUnit(Hub), TEXT("J_KillHub"));
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("J_HubDeathDoesNotFinish"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 10: // K annihilation toggle false
	{
		GM->DebugSetAnnihilationCountsAsWin(false);
		Expect(GPMatchWinLoseDebug::KillUnit(MainBaseAWeak.Get()), TEXT("K_KillMainBase"));
		Expect(GPMatchWinLoseDebug::IsPlaying(GS), TEXT("K_ToggleFalseDoesNotFinish"));
		GM->DebugSetAnnihilationCountsAsWin(true);
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 11: // I annihilation MainBase team1 death → team2
	{
		if (!Expect(ResetAndStartMatch(), TEXT("I_Restart")))
		{
			Finish();
			return;
		}
		if (!MainBaseAWeak.IsValid() || (MainBaseAWeak.Get() && MainBaseAWeak->IsDead()))
		{
			MainBaseAWeak = GPMatchWinLoseDebug::SpawnTeamMainBase(World, GPMatchWinLoseDebug::TeamA, PadA);
		}
		if (!MainBaseBWeak.IsValid() || (MainBaseBWeak.Get() && MainBaseBWeak->IsDead()))
		{
			MainBaseBWeak = GPMatchWinLoseDebug::SpawnTeamMainBase(World, GPMatchWinLoseDebug::TeamB, PadB);
		}
		Expect(GPMatchWinLoseDebug::KillUnit(MainBaseAWeak.Get()), TEXT("I_KillTeamAMainBase"));
		Expect(GPMatchWinLoseDebug::IsFinished(GS), TEXT("I_Finished"));
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamB, TEXT("I_TeamBWins"));
		Expect(GS->GetWinReasonTag() == Tags.Match_WinReason_Annihilation, TEXT("I_ReasonAnnihilation"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 12: // N finished gates economic orders
	{
		FGP_UnitDropManifest Manifest;
		Manifest.WorkerCount = 1;
		const GPUnitDropAuthority::FEvalResult UnitReject =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, TeamBStateWeak.Get(), Manifest);
		Expect(!UnitReject.bAccepted && UnitReject.RejectReason == EGP_UnitDropRejectReason::MatchFinished,
			TEXT("N_UnitDropMatchFinished"));

		const GPBuildingDropAuthority::FPurchaseResult PurchaseReject =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(
				World, TeamBStateWeak.Get(), EGP_OrbitalBuildingType::LogisticsHub);
		Expect(!PurchaseReject.bAccepted && PurchaseReject.RejectReason == EGP_BuildingDropRejectReason::MatchFinished,
			TEXT("N_PurchaseMatchFinished"));

		const GPBuildingDropAuthority::FDeployResult DeployReject =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				TeamBStateWeak.Get(),
				EGP_OrbitalBuildingType::LogisticsHub,
				FTransform(PadB));
		Expect(!DeployReject.bAccepted && DeployReject.RejectReason == EGP_BuildingDropRejectReason::MatchFinished,
			TEXT("N_DeployMatchFinished"));

		if (AGP_MainBase* Base = MainBaseBWeak.Get())
		{
			if (UGP_StorageComponent* Storage = Base->GetStorageComponent())
			{
				const FGP_ContainerLaunchResult LaunchResult = Storage->TryLaunchReadyContainer();
				Expect(!LaunchResult.bAccepted && LaunchResult.RejectReason == EGP_ContainerLaunchRejectReason::MatchFinished,
					TEXT("N_LaunchMatchFinished"));
			}
			else
			{
				Expect(false, TEXT("N_LaunchStoragePresent"));
			}
		}
		else
		{
			Expect(false, TEXT("N_LaunchBasePresent"));
		}
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 13: // O HUD seams readable from replicated facts
	{
		UGP_TEMP_S28P_PlanetaryFerroniteHUD* HUD = nullptr;
		for (TObjectIterator<UGP_TEMP_S28P_PlanetaryFerroniteHUD> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				HUD = *It;
				break;
			}
		}
		if (HUD == nullptr)
		{
			HUD = CreateWidget<UGP_TEMP_S28P_PlanetaryFerroniteHUD>(World, UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass());
			if (HUD != nullptr)
			{
				HUD->AddToViewport(0);
			}
		}
		if (!Expect(HUD != nullptr, TEXT("O_HUDPresent")))
		{
			Finish();
			return;
		}

		const float LocalScore = TeamBStateWeak.IsValid() && TeamBStateWeak->GetPlayerAttributeSet()
			? TeamBStateWeak->GetPlayerAttributeSet()->GetFerroniteScore()
			: 0.0f;
		HUD->SetMatchPlayingDisplay(GS->GetMatchTimeRemaining(), LocalScore, GS->GetDeliveryQuotaFerroniteScore());
		HUD->SetMatchFinishedDisplay(true, GS->GetWinReasonTag(), GS->GetWinnerTeamId());
		const FString Status = HUD->GetMatchStatusTextForContract();
		Expect(Status.Contains(TEXT("SCORE")), TEXT("O_StatusHasScore"));
		Expect(Status.Contains(TEXT("5000")), TEXT("O_StatusHasQuota"));
		Expect(HUD->GetMatchResultTitleForContract() == TEXT("VICTORY"), TEXT("O_VictoryTitle"));
		Expect(HUD->GetMatchResultReasonForContract() == TEXT("Annihilation"), TEXT("O_ReasonLabel"));
		Expect(HUD->GetDisplayedWinnerTeamIdForContract() == GPMatchWinLoseDebug::TeamB, TEXT("O_WinnerTeamFromGS"));
		Expect(HUD->IsMatchResultVisibleForContract(), TEXT("O_ResultVisible"));
		Expect(GS->GetWinnerTeamId() == GPMatchWinLoseDebug::TeamB, TEXT("O_NoClientWinCalc"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_MatchWinLoseContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_MatchWinLoseContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_MatchWinLoseContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_MatchWinLoseContractTestRunner::AdvanceStage() {}
bool UGP_MatchWinLoseContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_MatchWinLoseContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_MatchWinLoseContractTestRunner::Finish() { bFinished = true; }
void UGP_MatchWinLoseContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_MatchWinLoseContractTestRunner::UnbindWorldCleanup() {}
void UGP_MatchWinLoseContractTestRunner::CleanupActors() {}
void UGP_MatchWinLoseContractTestRunner::RestoreLiveTeams() {}
bool UGP_MatchWinLoseContractTestRunner::ResetAndStartMatch() { return false; }
bool UGP_MatchWinLoseContractTestRunner::IsolateLivePlayableTeams() { return false; }

#endif
