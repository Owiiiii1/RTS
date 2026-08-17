// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPGameMode.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Camera/GPCameraPawn.h"
#include "Game/GPGameState.h"
#include "Game/GPMatchResult.h"
#include "GameFramework/PlayerController.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Tags/GPGameplayTags.h"
#include "Templates/TypeHash.h"
#include "TimerManager.h"

namespace GPMatchEvalPrivate
{
	struct FTeamSnap
	{
		int32 TeamId = -1;
		int32 FerroniteScore = 0;
		int32 OrbitalFerronite = 0;
		int32 CurrentUnits = 0;
	};

	static int32 ToScoreInt(float Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return 0;
		}
		return FMath::RoundToInt(Value);
	}

	static FTeamSnap MakeSnap(const AGP_PlayerState* PlayerState)
	{
		FTeamSnap Snap;
		if (PlayerState == nullptr)
		{
			return Snap;
		}

		Snap.TeamId = PlayerState->GetTeamId();
		if (const UGP_PlayerAttributeSet* Attr = PlayerState->GetPlayerAttributeSet())
		{
			Snap.FerroniteScore = ToScoreInt(Attr->GetFerroniteScore());
			Snap.OrbitalFerronite = ToScoreInt(Attr->GetOrbitalFerronite());
			Snap.CurrentUnits = FMath::Max(0, ToScoreInt(Attr->GetCurrentUnits()));
		}
		return Snap;
	}

	static uint32 MixSeedAndTeam(int32 MatchSeed, int32 TeamId)
	{
		return HashCombine(static_cast<uint32>(MatchSeed), GetTypeHash(TeamId));
	}

	static int32 PickByMatchSeed(int32 MatchSeed, const TArray<int32>& TeamIds)
	{
		int32 BestTeam = INDEX_NONE;
		uint32 BestHash = 0;
		for (const int32 TeamId : TeamIds)
		{
			const uint32 Mixed = MixSeedAndTeam(MatchSeed, TeamId);
			if (BestTeam == INDEX_NONE
				|| Mixed > BestHash
				|| (Mixed == BestHash && TeamId < BestTeam))
			{
				BestTeam = TeamId;
				BestHash = Mixed;
			}
		}
		return BestTeam;
	}

	static int32 ResolveWinnerTeamId(const TArray<FTeamSnap>& Candidates, int32 MatchSeed)
	{
		if (Candidates.Num() == 0)
		{
			return INDEX_NONE;
		}
		if (Candidates.Num() == 1)
		{
			return Candidates[0].TeamId;
		}

		int32 BestScore = MIN_int32;
		for (const FTeamSnap& Candidate : Candidates)
		{
			BestScore = FMath::Max(BestScore, Candidate.FerroniteScore);
		}

		TArray<FTeamSnap> ScoreTier;
		for (const FTeamSnap& Candidate : Candidates)
		{
			if (Candidate.FerroniteScore == BestScore)
			{
				ScoreTier.Add(Candidate);
			}
		}
		if (ScoreTier.Num() == 1)
		{
			return ScoreTier[0].TeamId;
		}

		int32 BestOrbital = MIN_int32;
		for (const FTeamSnap& Candidate : ScoreTier)
		{
			BestOrbital = FMath::Max(BestOrbital, Candidate.OrbitalFerronite);
		}

		TArray<FTeamSnap> OrbitalTier;
		for (const FTeamSnap& Candidate : ScoreTier)
		{
			if (Candidate.OrbitalFerronite == BestOrbital)
			{
				OrbitalTier.Add(Candidate);
			}
		}
		if (OrbitalTier.Num() == 1)
		{
			return OrbitalTier[0].TeamId;
		}

		int32 BestUnits = MIN_int32;
		for (const FTeamSnap& Candidate : OrbitalTier)
		{
			BestUnits = FMath::Max(BestUnits, Candidate.CurrentUnits);
		}

		TArray<FTeamSnap> UnitTier;
		for (const FTeamSnap& Candidate : OrbitalTier)
		{
			if (Candidate.CurrentUnits == BestUnits)
			{
				UnitTier.Add(Candidate);
			}
		}
		if (UnitTier.Num() == 1)
		{
			return UnitTier[0].TeamId;
		}

		TArray<int32> TiedTeamIds;
		TiedTeamIds.Reserve(UnitTier.Num());
		for (const FTeamSnap& Candidate : UnitTier)
		{
			TiedTeamIds.Add(Candidate.TeamId);
		}
		return PickByMatchSeed(MatchSeed, TiedTeamIds);
	}
}

AGP_GameMode::AGP_GameMode()
{
	GameStateClass = AGP_GameState::StaticClass();
	PlayerControllerClass = AGP_PlayerController::StaticClass();
	PlayerStateClass = AGP_PlayerState::StaticClass();
	DefaultPawnClass = AGP_CameraPawn::StaticClass();
	PrimaryActorTick.bCanEverTick = false;
	MatchDurationSeconds = 600.0f;
	DeliveryQuotaFerroniteScore = 5000.0f;
	bAnnihilationCountsAsWin = true;
	ExpectedHumanPlayers = 2;
}

AGP_GameState* AGP_GameMode::GetGPGameState() const
{
	return GetGameState<AGP_GameState>();
}

int32 AGP_GameMode::GetConnectedHumanPlayerCount() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController != nullptr)
		{
			++Count;
		}
	}
	return Count;
}

bool AGP_GameMode::IsWinReasonBranchTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return false;
	}

	const FGameplayTag WinReasonRoot = FGPGameplayTags::Get().Match_WinReason_DeliveryQuota.RequestDirectParent();
	return WinReasonRoot.IsValid() && Tag.MatchesTag(WinReasonRoot);
}

void AGP_GameMode::PublishMatchConfigToGameState()
{
	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		return;
	}

	GPGameState->SetDeliveryQuotaFerroniteScore(DeliveryQuotaFerroniteScore);
	GPGameState->SetAnnihilationCountsAsWin(bAnnihilationCountsAsWin);
}

void AGP_GameMode::InitializeMatchSeed()
{
	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		return;
	}

	int32 NewSeed = static_cast<int32>(HashCombine(
		GetTypeHash(FDateTime::UtcNow().GetTicks()),
		GetUniqueID()));
	if (NewSeed == 0)
	{
		NewSeed = 1;
	}
	GPGameState->SetMatchSeed(NewSeed);
}

void AGP_GameMode::GatherPlayablePlayerStates(TArray<AGP_PlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();
	const AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		return;
	}

	TSet<int32> SeenTeams;
	for (APlayerState* Candidate : GPGameState->PlayerArray)
	{
		AGP_PlayerState* GPPlayerState = Cast<AGP_PlayerState>(Candidate);
		if (!IsValid(GPPlayerState))
		{
			continue;
		}

		const int32 TeamId = GPPlayerState->GetTeamId();
		if (TeamId < 1 || SeenTeams.Contains(TeamId))
		{
			continue;
		}

		SeenTeams.Add(TeamId);
		OutPlayerStates.Add(GPPlayerState);
	}

	OutPlayerStates.Sort([](const AGP_PlayerState& A, const AGP_PlayerState& B)
	{
		return A.GetTeamId() < B.GetTeamId();
	});
}

void AGP_GameMode::CaptureFinalScores(TArray<FGP_MatchTeamScore>& OutScores) const
{
	OutScores.Reset();
	TArray<AGP_PlayerState*> Playable;
	GatherPlayablePlayerStates(Playable);
	OutScores.Reserve(Playable.Num());
	for (const AGP_PlayerState* PlayerState : Playable)
	{
		const GPMatchEvalPrivate::FTeamSnap Snap = GPMatchEvalPrivate::MakeSnap(PlayerState);
		FGP_MatchTeamScore Entry;
		Entry.TeamId = Snap.TeamId;
		Entry.FerroniteScore = static_cast<float>(Snap.FerroniteScore);
		Entry.OrbitalFerronite = static_cast<float>(Snap.OrbitalFerronite);
		Entry.CurrentUnits = Snap.CurrentUnits;
		OutScores.Add(Entry);
	}
}

float AGP_GameMode::ComputeElapsedMatchSeconds() const
{
	const float Duration = FMath::Max(0.0f, MatchDurationSeconds);
	const AGP_GameState* GPGameState = GetGPGameState();
	const float Remaining = GPGameState != nullptr ? GPGameState->GetMatchTimeRemaining() : 0.0f;
	return FMath::Max(0.0f, Duration - Remaining);
}

int32 AGP_GameMode::ResolveWinnerTeamIdAmongPlayable() const
{
	TArray<AGP_PlayerState*> Playable;
	GatherPlayablePlayerStates(Playable);
	TArray<GPMatchEvalPrivate::FTeamSnap> Snaps;
	Snaps.Reserve(Playable.Num());
	for (const AGP_PlayerState* PlayerState : Playable)
	{
		Snaps.Add(GPMatchEvalPrivate::MakeSnap(PlayerState));
	}

	const AGP_GameState* GPGameState = GetGPGameState();
	const int32 Seed = GPGameState != nullptr ? GPGameState->GetMatchSeed() : 0;
	return GPMatchEvalPrivate::ResolveWinnerTeamId(Snaps, Seed);
}

void AGP_GameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameMode::BeginPlay: AGP_GameState unavailable (GameStateClass must be AGP_GameState). Match flow not initialized."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	GPGameState->SetMatchStateTag(GPTags.Match_State_WaitingForPlayers);
	GPGameState->SetMatchTimeRemaining(0.0f);
	GPGameState->ClearMatchResult();
	PublishMatchConfigToGameState();

	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::BeginPlay: match flow initialized to WaitingForPlayers (ExpectedHumanPlayers=%d, MatchDurationSeconds=%.1f, Quota=%.0f, Annihilation=%s)."),
		ExpectedHumanPlayers,
		MatchDurationSeconds,
		DeliveryQuotaFerroniteScore,
		bAnnihilationCountsAsWin ? TEXT("true") : TEXT("false"));
}

void AGP_GameMode::AssignPlayableTeamId(APlayerController* NewPlayer)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewPlayer == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::AssignPlayableTeamId: null PlayerController."));
		return;
	}

	AGP_PlayerState* GPPlayerState = NewPlayer->GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::AssignPlayableTeamId: missing AGP_PlayerState on '%s'."),
			*GetNameSafe(NewPlayer));
		return;
	}

	const int32 ExistingTeamId = GPPlayerState->GetTeamId();
	if (ExistingTeamId >= 1)
	{
		if (ExistingTeamId < MAX_int32)
		{
			NextPlayableTeamId = FMath::Max(NextPlayableTeamId, ExistingTeamId + 1);
		}

		UE_LOG(LogTemp, Verbose,
			TEXT("AGP_GameMode::AssignPlayableTeamId: Player='%s' already has playable TeamId=%d; preserved."),
			*GetNameSafe(NewPlayer), ExistingTeamId);
		return;
	}

	if (NextPlayableTeamId < 1 || NextPlayableTeamId >= MAX_int32)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameMode::AssignPlayableTeamId: allocator exhausted (NextPlayableTeamId=%d); Player='%s' left unassigned."),
			NextPlayableTeamId, *GetNameSafe(NewPlayer));
		return;
	}

	const int32 AssignedTeamId = NextPlayableTeamId;
	++NextPlayableTeamId;

	GPPlayerState->SetTeamId(AssignedTeamId);

	const int32 ConfirmedTeamId = GPPlayerState->GetTeamId();
	if (ConfirmedTeamId != AssignedTeamId)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::AssignPlayableTeamId: assignment failed for Player='%s' (expected=%d, actual=%d)."),
			*GetNameSafe(NewPlayer), AssignedTeamId, ConfirmedTeamId);
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::AssignPlayableTeamId: Player='%s' assigned TeamId=%d."),
		*GetNameSafe(NewPlayer), ConfirmedTeamId);
}

void AGP_GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	AssignPlayableTeamId(NewPlayer);

	const int32 HumanCount = GetConnectedHumanPlayerCount();
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::PostLogin: human players now %d (ExpectedHumanPlayers=%d)."),
		HumanCount, ExpectedHumanPlayers);

	TryStartMatch();
}

void AGP_GameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

	const int32 HumanCount = GetConnectedHumanPlayerCount();
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::Logout: human players now %d. OpponentDisconnect victory is deferred (GP-S34W)."),
		HumanCount);

	TryStartMatch();
}

void AGP_GameMode::TryStartMatch()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::TryStartMatch denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::TryStartMatch: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag CurrentState = GPGameState->GetMatchStateTag();
	if (CurrentState == GPTags.Match_State_Playing || CurrentState == GPTags.Match_State_Finished)
	{
		return;
	}

	const int32 Threshold = FMath::Max(1, ExpectedHumanPlayers);
	const int32 HumanCount = GetConnectedHumanPlayerCount();
	if (HumanCount >= Threshold)
	{
		StartMatchFlow();
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("AGP_GameMode::TryStartMatch: waiting for humans (%d / %d)."),
			HumanCount, Threshold);
	}
}

void AGP_GameMode::StartMatchFlow()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::StartMatchFlow denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::StartMatchFlow: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag CurrentState = GPGameState->GetMatchStateTag();
	if (CurrentState == GPTags.Match_State_Playing || CurrentState == GPTags.Match_State_Finished)
	{
		return;
	}

	StopMatchCountdown();
	bTimeoutEvaluationTriggered = false;

	GPGameState->ClearMatchResult();
	PublishMatchConfigToGameState();
	InitializeMatchSeed();
	GPGameState->SetMatchStateTag(GPTags.Match_State_Playing);

	const float InitialDuration = FMath::Max(0.0f, MatchDurationSeconds);
	GPGameState->SetMatchTimeRemaining(InitialDuration);

	if (InitialDuration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MatchCountdownHandle,
				this,
				&AGP_GameMode::HandleMatchCountdownTick,
				1.0f,
				true,
				1.0f);
		}
	}
	else
	{
		HandleMatchTimeExpired();
	}

	OnMatchFlowStarted();
}

void AGP_GameMode::StopMatchCountdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchCountdownHandle);
	}
	MatchCountdownHandle.Invalidate();
}

void AGP_GameMode::HandleMatchCountdownTick()
{
	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		StopMatchCountdown();
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPGameState->GetMatchStateTag() != GPTags.Match_State_Playing)
	{
		StopMatchCountdown();
		return;
	}

	const float CurrentRemaining = GPGameState->GetMatchTimeRemaining();
	const float NewRemaining = FMath::Max(0.0f, CurrentRemaining - 1.0f);
	GPGameState->SetMatchTimeRemaining(NewRemaining);

	UE_LOG(LogTemp, Verbose, TEXT("AGP_GameMode::HandleMatchCountdownTick: MatchTimeRemaining=%.1f"), NewRemaining);

	if (NewRemaining <= 0.0f)
	{
		HandleMatchTimeExpired();
	}
}

void AGP_GameMode::HandleMatchTimeExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bTimeoutEvaluationTriggered)
	{
		return;
	}

	bTimeoutEvaluationTriggered = true;
	StopMatchCountdown();

	if (AGP_GameState* GPGameState = GetGPGameState())
	{
		GPGameState->SetMatchTimeRemaining(0.0f);
	}

	EvaluateAndFinishMatch();
}

void AGP_GameMode::EvaluateQuotaVictory()
{
	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr || GPGameState->GetMatchStateTag() != FGPGameplayTags::Get().Match_State_Playing)
	{
		return;
	}

	TArray<AGP_PlayerState*> Playable;
	GatherPlayablePlayerStates(Playable);

	const int32 Quota = GPMatchEvalPrivate::ToScoreInt(GPGameState->GetDeliveryQuotaFerroniteScore());
	TArray<GPMatchEvalPrivate::FTeamSnap> QuotaValid;
	for (const AGP_PlayerState* PlayerState : Playable)
	{
		const GPMatchEvalPrivate::FTeamSnap Snap = GPMatchEvalPrivate::MakeSnap(PlayerState);
		if (Snap.FerroniteScore >= Quota)
		{
			QuotaValid.Add(Snap);
		}
	}

	if (QuotaValid.Num() == 0)
	{
		return;
	}

	const int32 WinnerTeamId = GPMatchEvalPrivate::ResolveWinnerTeamId(QuotaValid, GPGameState->GetMatchSeed());
	if (WinnerTeamId < 1)
	{
		return;
	}

	FinishMatch(WinnerTeamId, FGPGameplayTags::Get().Match_WinReason_DeliveryQuota);
}

void AGP_GameMode::NotifyFerroniteScoreChanged(AGP_PlayerState* SourcePlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	(void)SourcePlayerState;
	EvaluateQuotaVictory();
}

void AGP_GameMode::NotifyMainBaseDied(AGP_MainBase* DeadMainBase)
{
	if (!HasAuthority() || !IsValid(DeadMainBase))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr
		|| GPGameState->GetMatchStateTag() != FGPGameplayTags::Get().Match_State_Playing)
	{
		return;
	}

	if (!GPGameState->GetAnnihilationCountsAsWin())
	{
		UE_LOG(LogTemp, Log,
			TEXT("AGP_GameMode::NotifyMainBaseDied: MainBase='%s' TeamId=%d died but bAnnihilationCountsAsWin=false; Spectating deferred, match continues."),
			*GetNameSafe(DeadMainBase),
			DeadMainBase->GetTeamId());
		return;
	}

	const int32 DeadTeamId = DeadMainBase->GetTeamId();
	if (DeadTeamId < 1)
	{
		return;
	}

	TArray<AGP_PlayerState*> Playable;
	GatherPlayablePlayerStates(Playable);
	TArray<int32> OpponentTeamIds;
	for (const AGP_PlayerState* PlayerState : Playable)
	{
		const int32 TeamId = PlayerState->GetTeamId();
		if (TeamId >= 1 && TeamId != DeadTeamId)
		{
			OpponentTeamIds.AddUnique(TeamId);
		}
	}

	if (OpponentTeamIds.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::NotifyMainBaseDied: no opposing playable team for dead TeamId=%d; not inventing a winner."),
			DeadTeamId);
		return;
	}

	int32 WinnerTeamId = OpponentTeamIds[0];
	if (OpponentTeamIds.Num() > 1)
	{
		TArray<GPMatchEvalPrivate::FTeamSnap> OpponentSnaps;
		for (const AGP_PlayerState* PlayerState : Playable)
		{
			if (OpponentTeamIds.Contains(PlayerState->GetTeamId()))
			{
				OpponentSnaps.Add(GPMatchEvalPrivate::MakeSnap(PlayerState));
			}
		}
		WinnerTeamId = GPMatchEvalPrivate::ResolveWinnerTeamId(OpponentSnaps, GPGameState->GetMatchSeed());
	}

	if (WinnerTeamId < 1)
	{
		return;
	}

	FinishMatch(WinnerTeamId, FGPGameplayTags::Get().Match_WinReason_Annihilation);
}

void AGP_GameMode::EvaluateAndFinishMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::EvaluateAndFinishMatch: AGP_GameState unavailable."));
		return;
	}

	if (GPGameState->GetMatchStateTag() != FGPGameplayTags::Get().Match_State_Playing)
	{
		return;
	}

	TArray<AGP_PlayerState*> Playable;
	GatherPlayablePlayerStates(Playable);
	if (Playable.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameMode::EvaluateAndFinishMatch: invalid match configuration — no playable TeamId>=1 PlayerState. Not inventing a winner."));
		return;
	}

	const int32 WinnerTeamId = ResolveWinnerTeamIdAmongPlayable();
	if (WinnerTeamId < 1)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameMode::EvaluateAndFinishMatch: could not resolve a winner from %d playable team(s)."),
			Playable.Num());
		return;
	}

	FinishMatch(WinnerTeamId, FGPGameplayTags::Get().Match_WinReason_TimerScore);
}

void AGP_GameMode::FinishMatch(int32 InWinnerTeamId, FGameplayTag InWinReasonTag)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::FinishMatch denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::FinishMatch: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPGameState->GetMatchStateTag() == GPTags.Match_State_Finished)
	{
		return;
	}

	if (InWinnerTeamId < -1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::FinishMatch rejected WinnerTeamId=%d (must be >= -1)."),
			InWinnerTeamId);
		return;
	}

	if (!IsWinReasonBranchTag(InWinReasonTag))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::FinishMatch rejected invalid or non-GP.Match.WinReason tag '%s'."),
			*InWinReasonTag.ToString());
		return;
	}

	FGP_MatchResult Result;
	CaptureFinalScores(Result.FinalScores);
	Result.WinnerTeamId = InWinnerTeamId;
	Result.WinnerReason = InWinReasonTag;
	Result.MatchDuration = ComputeElapsedMatchSeconds();

	StopMatchCountdown();
	GPGameState->SetMatchResult(Result);
	GPGameState->SetMatchStateTag(GPTags.Match_State_Finished);
	OnMatchFlowFinished(InWinnerTeamId, InWinReasonTag);
}

void AGP_GameMode::OnMatchFlowStarted()
{
	UE_LOG(LogTemp, Log, TEXT("AGP_GameMode::OnMatchFlowStarted: match is Playing; countdown owned by GameMode."));
}

void AGP_GameMode::OnMatchFlowFinished(int32 WinnerTeamId, FGameplayTag WinReasonTag)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::OnMatchFlowFinished: WinnerTeamId=%d WinReason=%s"),
		WinnerTeamId, *WinReasonTag.ToString());
}

#if !UE_BUILD_SHIPPING
void AGP_GameMode::DebugSetMatchSeed(int32 InMatchSeed)
{
	if (!HasAuthority())
	{
		return;
	}
	if (AGP_GameState* GPGameState = GetGPGameState())
	{
		GPGameState->SetMatchSeed(InMatchSeed);
	}
}

void AGP_GameMode::DebugSetDeliveryQuotaFerroniteScore(float InQuota)
{
	if (!HasAuthority())
	{
		return;
	}
	DeliveryQuotaFerroniteScore = FMath::Max(0.0f, InQuota);
	PublishMatchConfigToGameState();
}

void AGP_GameMode::DebugSetAnnihilationCountsAsWin(bool bInAnnihilationCountsAsWin)
{
	if (!HasAuthority())
	{
		return;
	}
	bAnnihilationCountsAsWin = bInAnnihilationCountsAsWin;
	PublishMatchConfigToGameState();
}

void AGP_GameMode::DebugResetMatchFlowToWaiting()
{
	if (!HasAuthority())
	{
		return;
	}

	StopMatchCountdown();
	bTimeoutEvaluationTriggered = false;
	if (AGP_GameState* GPGameState = GetGPGameState())
	{
		GPGameState->SetMatchStateTag(FGPGameplayTags::Get().Match_State_WaitingForPlayers);
		GPGameState->SetMatchTimeRemaining(0.0f);
		GPGameState->ClearMatchResult();
		PublishMatchConfigToGameState();
	}
}

void AGP_GameMode::DebugStartMatchFlow()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("GP Match Debug: DebugStart denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("GP Match Debug: DebugStart missing AGP_GameState."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag CurrentState = GPGameState->GetMatchStateTag();
	if (CurrentState == GPTags.Match_State_Playing)
	{
		UE_LOG(LogTemp, Log,
			TEXT("GP Match Debug: DebugStart no-op — already Playing (ExpectedHumanPlayers=%d unchanged)."),
			ExpectedHumanPlayers);
		return;
	}

	if (CurrentState == GPTags.Match_State_Finished)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Match Debug: DebugStart rejected — match is Finished (WinnerTeamId=%d). ExpectedHumanPlayers=%d unchanged."),
			GPGameState->GetWinnerTeamId(),
			ExpectedHumanPlayers);
		return;
	}

	if (CurrentState != GPTags.Match_State_WaitingForPlayers)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Match Debug: DebugStart rejected — MatchState=%s (only WaitingForPlayers can debug-start)."),
			*CurrentState.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("GP Match Debug: DebugStart invoking StartMatchFlow from WaitingForPlayers (ExpectedHumanPlayers=%d unchanged; not a production auto-start)."),
		ExpectedHumanPlayers);
	StartMatchFlow();
}
#endif
